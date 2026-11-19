#ifndef AMMO_BUYBACK_PRICE_H
#define AMMO_BUYBACK_PRICE_H

// 2Ship a5cbfdf1c: arrows always sell at full price. Header-only so mm_upstream_fixes_1754_test.cpp can test the
// decision without naming a symbol AmmoBuyback.cpp defines: such a reference would pull that registrar TU into the
// link by itself, and MMRegistrarCoverage could no longer see it dropped from ship__Enh_registrars.

#include "2s2h/Enhancements/Enhancements.h"
#include "z64item.h"

static inline int AmmoBuyback_SalePriceFor(int mode, int itemId, int count, int pricePerUnit) {
    int totalPrice = count * pricePerUnit;
    if (mode == AMMO_BUYBACK_HALF_PRICE && itemId != ITEM_BOW) {
        totalPrice = (totalPrice + 1) / 2;
    }
    return totalPrice;
}

#endif // AMMO_BUYBACK_PRICE_H
