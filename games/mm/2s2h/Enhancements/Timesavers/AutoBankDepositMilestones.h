#ifndef AUTO_BANK_DEPOSIT_MILESTONES_H
#define AUTO_BANK_DEPOSIT_MILESTONES_H

// 2Ship 24df3b56c: Auto Bank Deposit notifies only when a deposit takes the balance to or past one of these milestones.
// Header-only so mm_upstream_fixes_1754_test.cpp can test the decision without naming a symbol AutoBankDeposit.cpp
// defines: such a reference would pull that registrar TU into the link by itself, and MMRegistrarCoverage could no
// longer see it dropped from ship__Enh_registrars.

#include <stdbool.h>

static inline bool AutoBankDeposit_ReachesNotificationMilestone(int balanceBeforeDeposit, int balanceAfterDeposit) {
    static const int kAutoBankDepositMilestones[] = { 100, 200, 500, 1000, 5000 };

    for (int i = 0; i < (int)(sizeof(kAutoBankDepositMilestones) / sizeof(kAutoBankDepositMilestones[0])); i++) {
        if (balanceBeforeDeposit < kAutoBankDepositMilestones[i] &&
            balanceAfterDeposit >= kAutoBankDepositMilestones[i]) {
            return true;
        }
    }

    return false;
}

#endif // AUTO_BANK_DEPOSIT_MILESTONES_H
