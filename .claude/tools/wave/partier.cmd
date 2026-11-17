@echo off
rem partier.cmd <tag> [--build-dir=<NAME>] [-j <N>] [--main=<DIR>] [--state-dir=<DIR>]
rem Runs a BUILT tree's rando tier at -j 1 and then -j N (default 2) and exits 0
rem only when the two runs agree (#1692). Needs the gate lock held by <tag> and
rem the build slot. Writes <state>/logs/partier-<tag>.log. See README.md.
setlocal
set "SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN=1"
call "%~dp0_python.cmd" || exit /b 9
%WAVE_PY% "%~dp0partier.py" %*
exit /b %ERRORLEVEL%
