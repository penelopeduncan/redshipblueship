/**
 * @file headless_crash.cpp
 * @brief Unattended-safe crash handling for the single executable (issue #388)
 *
 * See headless_crash.h for the rationale. The short version: libultraship's
 * crash handlers end in a modal dialog, and a modal dialog on a runner is an
 * infinite loop. Here a fatal fault instead produces a few lines on stderr and
 * an immediate non-zero exit.
 *
 * Everything on the fault path is written with raw write(2)/_write, not
 * printf: stdio locks can already be held by the thread we just interrupted,
 * and a crash handler that deadlocks on a mutex is the same 180-second
 * timeout by a different route.
 */

#include "headless_crash.h"

// C headers, not their <c*> spellings: the fault path uses size_t/uintptr_t
// unqualified, and only the C forms are guaranteed to declare them in the
// global namespace.
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <ship/Context.h>

#include "integration_test_hooks.h"
#include "test_runner.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <ship/debug/CrashHandler.h>
#if defined(_MSC_VER)
// GetUserObjectInformationW (window-station visibility probe) lives in user32.
#pragma comment(lib, "user32.lib")
#endif
#else
#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/prctl.h>
// FaultPc's machine-context layout. Linux only: macOS's <ucontext.h> refuses to
// compile without _XOPEN_SOURCE, and FaultPc reads no Apple layout.
#include <ucontext.h>
#endif
#endif

namespace {

// How many frames the module+offset walk prints (#1690). The fault and fifteen
// callers reach from a game function to the row or frame loop that drove it.
constexpr int kModuleOffsetFrames = 16;

// --------------------------------------------------------------------------
// Async-signal-safe output primitives
// --------------------------------------------------------------------------

void RawWrite(const char* str, size_t len) {
#ifdef _WIN32
    _write(2, str, (unsigned int)len);
#else
    // Deliberately ignoring the result: there is nothing useful to do about a
    // short write from inside a crash handler, and we are about to _exit.
    ssize_t ignored = write(2, str, len);
    (void)ignored;
#endif
}

void RawWrite(const char* str) {
    RawWrite(str, strlen(str));
}

// Unsigned -> text without snprintf, which is not async-signal-safe.
void RawWriteNum(unsigned long long value, int base) {
    char buf[24];
    int i = (int)sizeof(buf);
    buf[--i] = '\0';
    if (value == 0) {
        buf[--i] = '0';
    }
    while (value != 0 && i > 0) {
        unsigned digit = (unsigned)(value % (unsigned)base);
        buf[--i] = (char)(digit < 10 ? '0' + digit : 'a' + (digit - 10));
        value /= (unsigned)base;
    }
    RawWrite(&buf[i]);
}

// --------------------------------------------------------------------------
// Headless detection
// --------------------------------------------------------------------------

bool EnvSet(const char* name) {
    const char* value = getenv(name);
    return value != nullptr && value[0] != '\0';
}

bool sForcedHeadless = false;

bool DetectHeadless() {
    // Explicit override wins in both directions. RSBS_HEADLESS=0 is the escape
    // hatch for a desktop user whose environment trips one of the heuristics.
    const char* forced = getenv("RSBS_HEADLESS");
    if (forced != nullptr && forced[0] != '\0') {
        return strcmp(forced, "0") != 0;
    }

    // An entry point that knows it is unattended (the unit-test harness) said
    // so explicitly.
    if (sForcedHeadless) {
        return true;
    }

    // Any test mode is unattended by construction — a test that waits for a
    // human is a test that times out.
    if (TestRunner_IsIntegrationTestMode()) {
        return true;
    }

    if (EnvSet("CI") || EnvSet("GITHUB_ACTIONS")) {
        return true;
    }

#ifdef _WIN32
    // The authoritative Windows question is not "is stderr a tty" (CI
    // redirects it) but "does this process have a desktop to draw on".
    // A non-visible window station — a service, or a session-0 job — makes
    // MessageBoxA unanswerable.
    HWINSTA station = GetProcessWindowStation();
    USEROBJECTFLAGS flags;
    DWORD needed = 0;
    if (station != nullptr && GetUserObjectInformationW(station, UOI_FLAGS, &flags, (DWORD)sizeof(flags), &needed)) {
        if ((flags.dwFlags & WSF_VISIBLE) == 0) {
            return true;
        }
    }
    return false;
#else
    // No display server at all: SDL cannot open a window, and its message box
    // fallback is what blocks in an X11 poll().
    return !EnvSet("DISPLAY") && !EnvSet("WAYLAND_DISPLAY");
#endif
}

// --------------------------------------------------------------------------
// Fault handlers
// --------------------------------------------------------------------------

#ifndef _WIN32

const char* SignalName(int sig) {
    switch (sig) {
        case SIGSEGV:
            return "SIGSEGV (invalid access to storage)";
        case SIGBUS:
            return "SIGBUS (bad memory access)";
        case SIGILL:
            return "SIGILL (illegal instruction)";
        case SIGFPE:
            return "SIGFPE (erroneous arithmetic operation)";
        case SIGABRT:
            return "SIGABRT (abort)";
        default:
            return "unknown fatal signal";
    }
}

// Writes "<module basename>+0x<offset from its load base>" for @p pc, or the
// raw address when no loaded module holds it (#1690). For the PIE executable
// the default toolchains build, the offset is what `addr2line -e redship` and
// redship.map take; on macOS `atos -o redship -l 0x100000000` adds the base.
//
// dladdr is not on the POSIX async-signal-safe list, but it reads the loader's
// tables without allocating on glibc and dyld, and the caller only reaches it
// after backtrace_symbols_fd's lines are already out.
void RawWriteModuleOffset(uintptr_t pc) {
    Dl_info info;
    if (dladdr(reinterpret_cast<void*>(pc), &info) != 0 && info.dli_fbase != nullptr) {
        const char* name = info.dli_fname != nullptr ? info.dli_fname : "";
        const char* slash = strrchr(name, '/');
        if (slash != nullptr) {
            name = slash + 1;
        }
        RawWrite(name[0] != '\0' ? name : "?");
        RawWrite("+0x");
        RawWriteNum((unsigned long long)(pc - (uintptr_t)info.dli_fbase), 16);
        return;
    }
    RawWrite("0x");
    RawWriteNum((unsigned long long)pc, 16);
    RawWrite(" (no module)");
}

// The interrupted program counter, from the signal frame's machine context;
// 0 where this build does not know the layout (then no fault line is printed,
// and the traceback still names the faulting frame).
uintptr_t FaultPc(void* ucontext) {
    if (ucontext == nullptr) {
        return 0;
    }
#if defined(__linux__) && defined(__x86_64__)
    return (uintptr_t) static_cast<ucontext_t*>(ucontext)->uc_mcontext.gregs[REG_RIP];
#elif defined(__linux__) && defined(__aarch64__)
    return (uintptr_t) static_cast<ucontext_t*>(ucontext)->uc_mcontext.pc;
#else
    return 0;
#endif
}

void HeadlessSignalHandler(int sig, siginfo_t* info, void* ucontext) {
    // Re-arm as SIG_DFL first. If anything below faults a second time the
    // process dies on the spot rather than recursing through this handler.
    signal(sig, SIG_DFL);
    // The paired integration row tees fd 2 through a pipe; write the report to
    // the real stderr, since the tee's reader may never run again.
    IntegrationTest_StderrCaptureRestoreForCrash();

    RawWrite("\n[CRASH] ");
    RawWrite(SignalName(sig));
    RawWrite(" (signal ");
    RawWriteNum((unsigned long long)sig, 10);
    RawWrite(")\n");

    if (info != nullptr && (sig == SIGSEGV || sig == SIGBUS)) {
        RawWrite("[CRASH] fault address: 0x");
        RawWriteNum((unsigned long long)(uintptr_t)info->si_addr, 16);
        // A small faulting address is a near-null dereference, and its value
        // is the struct field offset — worth reading literally.
        RawWrite("\n");
    }

    // backtrace_symbols_fd is the async-signal-safe member of the backtrace
    // family: it formats into the fd without allocating, unlike
    // backtrace_symbols. Symbols are mangled, but the frame addresses alone
    // resolve through addr2line, and that is infinitely more than a timeout
    // gives us today.
    void* frames[64];
    int count = backtrace(frames, (int)(sizeof(frames) / sizeof(frames[0])));
    RawWrite("[CRASH] traceback (");
    RawWriteNum((unsigned long long)count, 10);
    RawWrite(" frames):\n");
    backtrace_symbols_fd(frames, count, 2);

    // #1690: the same place as module+offset, which survives ASLR and resolves
    // through redship.map or addr2line with no debugger. After the lines above,
    // since dladdr is the one call here off the async-signal-safe list.
    const uintptr_t pc = FaultPc(ucontext);
    if (pc != 0) {
        RawWrite("[CRASH] fault at ");
        RawWriteModuleOffset(pc);
        RawWrite("\n");
    }
    RawWrite("[CRASH] frames as module+offset (return addresses; the traceback's order):\n");
    for (int i = 0; i < count && i < kModuleOffsetFrames; i++) {
        RawWrite("[CRASH] #");
        RawWriteNum((unsigned long long)i, 10);
        RawWrite(" ");
        RawWriteModuleOffset((uintptr_t)frames[i]);
        RawWrite("\n");
    }

    // Best effort, and deliberately last: this touches game state and is not
    // async-signal-safe, so it runs only after the breadcrumbs above have
    // already escaped to stderr.
    if (IntegrationTest_GetMode() == INT_TEST_GAMEPLAY_ROUNDTRIP) {
        IntegrationTest_LogGameplayState("signal");
    }

    RawWrite("[CRASH] exiting immediately (headless session; no crash dialog)\n");

    // 128 + signal is the convention the integration tier documents
    // (docs/ci-gameplay-repro-postmortem.md 7) so CI can tell "crashed" from
    // "test failed". _exit, not exit: static destructors on a corrupted heap
    // are how a crash turns back into a hang.
    _exit(128 + sig);
}

void InstallHandlers() {
    // Warm up backtrace() now. Its first call dlopens libgcc and allocates;
    // doing that lazily from inside a SIGSEGV handler can deadlock on the
    // loader lock.
    void* warmup[4];
    (void)backtrace(warmup, 4);
    // And dladdr (#1690), so the loader has resolved it before a fault needs it.
    Dl_info warmupInfo;
    (void)dladdr(static_cast<void*>(&sForcedHeadless), &warmupInfo);

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = HeadlessSignalHandler;
    sigemptyset(&action.sa_mask);

    sigaction(SIGSEGV, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
    sigaction(SIGILL, &action, nullptr);
    sigaction(SIGFPE, &action, nullptr);
    sigaction(SIGABRT, &action, nullptr);
}

#else // _WIN32

// Map the fatal Windows exception codes onto the POSIX signal numbers the
// integration tier documents, so a Windows crash reads the same way a Linux
// one does (exit >= 128 means crashed, signal = code - 128).
int MapExceptionToSignal(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_STACK_OVERFLOW:
            return 11; // SIGSEGV
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_PRIV_INSTRUCTION:
            return 4; // SIGILL
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_INT_OVERFLOW:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        case EXCEPTION_FLT_INVALID_OPERATION:
        case EXCEPTION_FLT_OVERFLOW:
        case EXCEPTION_FLT_UNDERFLOW:
        case EXCEPTION_FLT_INEXACT_RESULT:
        case EXCEPTION_FLT_DENORMAL_OPERAND:
        case EXCEPTION_FLT_STACK_CHECK:
            return 8; // SIGFPE
        default:
            return 6; // SIGABRT-equivalent bucket
    }
}

// The executable's base and file name, read once at install (#1690), so a frame
// in redship.exe is named without a loader call on the fault path.
uintptr_t sExeBase = 0;
char sExeName[64] = "";

// The ASCII basename of a wide module path, into a fixed buffer ('?' for any
// other character): the fault path has no heap and no code-page conversion.
void CopyAsciiBasename(const wchar_t* path, size_t len, char* out, size_t cap) {
    size_t start = 0;
    for (size_t i = 0; i < len; i++) {
        if (path[i] == L'\\' || path[i] == L'/') {
            start = i + 1;
        }
    }
    size_t o = 0;
    for (size_t i = start; i < len && o + 1 < cap; i++) {
        const wchar_t c = path[i];
        out[o++] = (c >= 0x20 && c < 0x7f) ? (char)c : '?';
    }
    out[o] = '\0';
}

// Writes "<module basename>+0x<offset from its base>" for @p pc, or the raw
// address when no loaded image holds it (#1690). An offset in redship.exe
// resolves through redship.map (#1303); one in SDL2.dll or a GL driver at
// least names the module. RtlPcToFileHeader reads the loader's inverted
// function table without allocating; only a frame outside the executable
// costs a GetModuleFileNameW, into a stack buffer.
void RawWriteModuleOffset(uintptr_t pc) {
    PVOID base = nullptr;
    if (RtlPcToFileHeader(reinterpret_cast<PVOID>(pc), &base) == nullptr || base == nullptr) {
        RawWrite("0x");
        RawWriteNum((unsigned long long)pc, 16);
        RawWrite(" (no module)");
        return;
    }
    if ((uintptr_t)base == sExeBase && sExeName[0] != '\0') {
        RawWrite(sExeName);
    } else {
        wchar_t path[MAX_PATH];
        const DWORD len = GetModuleFileNameW(static_cast<HMODULE>(base), path, MAX_PATH);
        char name[64];
        CopyAsciiBasename(path, (len > 0 && len < MAX_PATH) ? len : 0, name, sizeof(name));
        RawWrite(name[0] != '\0' ? name : "?");
    }
    RawWrite("+0x");
    RawWriteNum((unsigned long long)(pc - (uintptr_t)base), 16);
}

#if defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__)
// Walks the faulting thread's stack from the fault's own registers (#1690):
// #0 is the faulting instruction, each later frame a return address. A function
// with unwind data is unwound by RtlVirtualUnwind from its .pdata; a leaf with
// none has not moved RSP, so its return address is the top of the stack. The
// walk stops at a null return address, a stack pointer outside this thread's
// stack (read from the TEB, no API call) or a frame that does not move it. No
// DbgHelp, no heap, no stdio: the context copy and every buffer are on the stack.
void RawWriteUnwoundFrames(const CONTEXT* faultContext) {
    CONTEXT context = *faultContext;
    const NT_TIB* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    const DWORD64 stackLow = (DWORD64)(uintptr_t)tib->StackLimit;
    const DWORD64 stackHigh = (DWORD64)(uintptr_t)tib->StackBase;
    RawWrite("[CRASH] frames as module+offset (#0 the fault, then return addresses):\n");
    for (int i = 0; i < kModuleOffsetFrames; i++) {
        const DWORD64 pc = context.Rip;
        const DWORD64 sp = context.Rsp;
        // A null #0 is a call through a null pointer: printed, then unwound as
        // a leaf, which recovers the caller.
        if (i > 0 && pc == 0) {
            break;
        }
        RawWrite("[CRASH] #");
        RawWriteNum((unsigned long long)i, 10);
        RawWrite(" ");
        RawWriteModuleOffset((uintptr_t)pc);
        RawWrite("\n");
        if (sp < stackLow || sp + sizeof(DWORD64) > stackHigh) {
            break;
        }
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION entry = RtlLookupFunctionEntry(pc, &imageBase, nullptr);
        if (entry != nullptr) {
            PVOID handlerData = nullptr;
            DWORD64 establisherFrame = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, pc, entry, &context, &handlerData, &establisherFrame,
                             nullptr);
        } else {
            context.Rip = *reinterpret_cast<const DWORD64*>((uintptr_t)sp);
            context.Rsp = sp + sizeof(DWORD64);
        }
        if (context.Rsp <= sp) {
            break; // every real unwind pops at least the return address
        }
    }
}
#endif

LONG WINAPI HeadlessExceptionFilter(EXCEPTION_POINTERS* exceptionInfo) {
    const DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;
    // The paired integration row tees fd 2 through a pipe; write the report to
    // the real stderr, since the tee's reader may never run again.
    IntegrationTest_StderrCaptureRestoreForCrash();

    RawWrite("\n[CRASH] Windows exception: 0x");
    RawWriteNum((unsigned long long)code, 16);
    RawWrite("\n");

    // ASLR-independent fault location: an exe-relative offset survives across
    // runs and resolves to a function through the linker map (redship.map)
    // even without PDBs.
    {
        uintptr_t rip = (uintptr_t)exceptionInfo->ExceptionRecord->ExceptionAddress;
        uintptr_t base = (uintptr_t)GetModuleHandleA(nullptr);
        RawWrite("[CRASH] addr=0x");
        RawWriteNum((unsigned long long)rip, 16);
        RawWrite(" exe_base=0x");
        RawWriteNum((unsigned long long)base, 16);
        RawWrite(" exe_offset=0x");
        RawWriteNum((unsigned long long)(rip >= base ? rip - base : rip), 16);
        RawWrite("\n");
    }

    if (code == EXCEPTION_ACCESS_VIOLATION && exceptionInfo->ExceptionRecord->NumberParameters >= 2) {
        const ULONG_PTR* info = exceptionInfo->ExceptionRecord->ExceptionInformation;
        RawWrite("[CRASH] access violation: ");
        RawWrite(info[0] == 1 ? "WRITE" : (info[0] == 8 ? "EXEC" : "READ"));
        RawWrite(" at 0x");
        RawWriteNum((unsigned long long)info[1], 16);
        RawWrite("\n");
    }

    // #1690: the fault and its callers as module+offset, on stderr. The frame
    // dump below goes only to the log file on a release build (libultraship's
    // logger has no console sink there), so without these lines naming the
    // faulting function took a rebuild. Before anything that is not crash-safe.
    RawWrite("[CRASH] fault at ");
    RawWriteModuleOffset((uintptr_t)exceptionInfo->ExceptionRecord->ExceptionAddress);
    RawWrite("\n");
#if defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__)
    if (code == EXCEPTION_STACK_OVERFLOW) {
        // The walk needs a CONTEXT copy on a stack that has none left.
        RawWrite("[CRASH] frames not unwound: a stack overflow leaves no stack to walk with\n");
    } else {
        RawWriteUnwoundFrames(exceptionInfo->ContextRecord);
    }
#endif

    if (IntegrationTest_GetMode() == INT_TEST_GAMEPLAY_ROUNDTRIP) {
        IntegrationTest_LogGameplayState("signal");
    }

    // The same registers + traceback dump libultraship's own filter produces
    // (appended to the rotating log as a CRITICAL record, then flushed) —
    // just without the modal MessageBoxA that follows it upstream. Only with a
    // logger: PrintStack flushes it unchecked, and a context whose bring-up
    // never ran InitLogging has none, which would fault a second time here.
    auto ctx = Ship::Context::GetInstance();
    if (ctx != nullptr && ctx->GetCrashHandler() != nullptr && ctx->GetLogger() != nullptr) {
        ctx->GetCrashHandler()->PrintStack(exceptionInfo->ContextRecord);
    }

    RawWrite("[CRASH] exiting immediately (headless session; no crash dialog)\n");

    TerminateProcess(GetCurrentProcess(), (UINT)(128 + MapExceptionToSignal(code)));
    return EXCEPTION_EXECUTE_HANDLER; // unreachable
}

void InstallHandlers() {
    if (sExeBase == 0) {
        sExeBase = (uintptr_t)GetModuleHandleW(nullptr);
        wchar_t path[MAX_PATH];
        const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
        CopyAsciiBasename(path, (len > 0 && len < MAX_PATH) ? len : 0, sExeName, sizeof(sExeName));
    }
    SetUnhandledExceptionFilter(HeadlessExceptionFilter);
}

#endif // _WIN32

} // namespace

extern "C" void HeadlessCrash_ForceHeadless(void) {
    sForcedHeadless = true;
}

extern "C" int HeadlessCrash_IsHeadless(void) {
    // Cached: getenv is not async-signal-safe, and the verdict must not be
    // able to differ between install time and fault time.
    static const bool sHeadless = DetectHeadless();
    return sHeadless ? 1 : 0;
}

extern "C" void HeadlessCrash_SelfTestFault(void) {
    // Nothing may outlive the fault but its stderr lines. With the handlers in
    // place they end the process before the system's crash reporting runs;
    // these cover a build without them (the lock's red half), so that run
    // fails on its exit code instead of a dialog nobody can answer or a dump.
#ifdef _WIN32
    SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#else
    struct rlimit noCore;
    noCore.rlim_cur = 0;
    noCore.rlim_max = 0;
    (void)setrlimit(RLIMIT_CORE, &noCore);
#if defined(__linux__)
    (void)prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
#endif
#endif
    // Through a volatile pointer, so the compiler emits the store instead of
    // treating a known null dereference as unreachable.
    static int* volatile sTarget = nullptr;
    *sTarget = 0x1690;
}

extern "C" void HeadlessCrash_InstallForTestMode(void) {
    // No context exists yet and none is created here: a ROM-free row in the
    // all-tests process never gets one it did not bring up itself. A row that
    // later builds one runs HeadlessCrash_ClaimAndInstall, which claims
    // libultraship's handler and installs these handlers again over it.
    HeadlessCrash_ForceHeadless();
    if (!HeadlessCrash_IsHeadless()) {
        return; // RSBS_HEADLESS=0: the person asked for the OS's own handling.
    }
    InstallHandlers();
}

extern "C" void HeadlessCrash_ClaimAndInstall(void) {
    // Construct libultraship's CrashHandler now, on our terms. It is
    // idempotent, so the later calls from OoT's OTRGlobals and MM's BenPort
    // find it already built and skip their sigaction/SetUnhandledExceptionFilter
    // registration entirely. Without this the window between game init and
    // any later re-install is unprotected — and that window is exactly where
    // the #388 boot SIGSEGV fired.
    auto ctx = Ship::Context::GetInstance();
    if (ctx != nullptr) {
        (void)ctx->InitCrashHandler();
    }

    if (!HeadlessCrash_IsHeadless()) {
        return; // Desktop user: keep upstream's dialog and rich log dump.
    }

    InstallHandlers();
}
