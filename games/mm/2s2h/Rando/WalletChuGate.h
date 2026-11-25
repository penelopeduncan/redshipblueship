#ifndef RANDO_WALLET_CHU_GATE_H
#define RANDO_WALLET_CHU_GATE_H

/*
 * #1702 stage C2: Majora's Mask's questions about the pair's Child's Wallet and
 * Bombchu Bag (src/common/shared_resources.h, "THE PAIR'S POSSESSION"). Each
 * reads the FROZEN record only, never a CVar, a fileNum or a play state, so the
 * give, the logic engine, the HUD and a ROM-free row all reach the same answer.
 * Every answer is 0 on every released file (none holds the Shared Child's Wallet
 * rule or comboFlags 0x80), while nothing is frozen (an unpaired boot, the title
 * screen), and on every file a C2 build can create. NO CONSUMER READS THEM IN
 * STAGE C2: stage C3 is the first (ConvertItem, CAN_AFFORD, the HUD, the tracker
 * read MM_Combo_WalletGated, never RANDO_INF_OBTAINED_CHILD_WALLET alone).
 *
 * Defined in games/mm/2s2h/GameExports_SingleExe.cpp; included there and by
 * every caller, so each signature is checked against the definition.
 * C-compatible: some callers will be C (z_parameter.c in stage C3).
 */

#ifdef __cplusplus
extern "C" {
#endif

/** 1 while MM's first wallet is a Child's Wallet to be found: since C2 the frozen
 *  Shared Child's Wallet rule (RSBS_XITEM2_SHARED_CHILD_WALLET); stage C3 ORs in
 *  MM's own frozen Shuffle Child's Wallet option. 0 otherwise. First used by C3. */
int MM_Combo_ChildWalletShuffled(void);

/** 1 while MM must act as if it held no wallet: MM_Combo_ChildWalletShuffled and
 *  MM's RANDO_INF_OBTAINED_CHILD_WALLET is clear. 0 whenever the shuffle is off,
 *  whatever the flag holds (released files may hold it set or clear). The one
 *  predicate stage C3's consumers read. */
int MM_Combo_WalletGated(void);

/** 1 while this file shares one Bombchu Bag between the two games (the frozen
 *  comboFlags RSBS_COMBO_FLAG_SHARED_BOMBCHU_BAG, Combo_ComboBombchuBagShared).
 *  0 on every loadable file until stage C8 allocates the bit; C8 is the first
 *  to read it (MM's Bombchu licence and capacity). */
int MM_Combo_ChuBagShared(void);

#ifdef __cplusplus
}
#endif

#endif // RANDO_WALLET_CHU_GATE_H
