/**
 * @file SohMenuHostedMmPlayer.cpp
 * @brief Majora's Mask's Player, Equipment and PlayerActions rows, hosted on SoH's Enhancements pages through the
 *        hosted-row seam (#1236, lane W6-PLAYER).
 *
 * Twelve 2Ship rows whose only widgets sat in the excluded BenMenu.cpp, so nothing in the single exe wrote their keys
 * and none of their TUs was linked (games/mm/CMakeLists.txt carves them into ship__Enh_registrars beside this change).
 * Each row binds its RSBS_CVAR_MM_* define (src/common/cvar_shared_keys.h), and SoH's page calls each function below
 * in one line:
 *  - Quality of Life > Controls: a "Majora's Mask" block (Manual Jump, Prevent Diving Over Water, Infinite Deku
 *    Hopping, and since #1411 Invert Zora Swim Y Axis). SoH has no twin of any of them. Invert Zora Swim Y Axis
 *    (2Ship dabc3aadc) has no TU: MM reads its key inline in GameInteractor_InvertControl (GameExports_SingleExe.cpp)
 *    with default 1, which is vanilla, so the row declares DefaultValue(true) and draws ticked when unset.
 *  - Skips & Speed-ups > Animations: "MM Vine/Ladder Climb Speed" right after SoH's own. The pair is held apart
 *    (kMustStayDistinct): SoH adds an offset (+0-12), MM multiplies (1-5x). Faster Push/Pull right after SoH's Block
 *    Pushing Speed, its nearest relative (one switch over every pushed and pulled object, not a speed step). A
 *    "Majora's Mask" block at the end of the section for Fast Deku Flower Launch and Fierce Deity Putaway.
 *  - Items > Equipment: a "Majora's Mask" block (Two-Handed Sword Spin Attack, Active Item on B (2Ship #1870,
 *    replacing Great Fairy Sword B-Button Attack), Better Picto Message, and since 2Ship #1989 Hot Water Never Cools,
 *    an Items row in 2Ship with no SoH twin: Ocarina of Time has no Hot Spring Water).
 *  - Items > Boomerang: Instant Fin Boomerangs Recall right after SoH's Instant Boomerang Recall, its nearest relative
 *    (Zora Link's fins and the B button, not the Boomerang and its item button).
 *  - Items > Hookshot: since 2Ship #1951 a "Majora's Mask" block at the end of the section, after SoH's Targetable
 *    Hookshot Reticle, for Faster Hookshot (a Player row in 2Ship; SoH has no hookshot speed setting to sit beside).
 *  - Difficulty > Drops: "MM Enable Bombchu Drops" right after SoH's own. Held apart (kMustStayDistinct): SoH's rule
 *    weighs what Link carries and an Ocarina of Time seed can force it on; MM's gives a Bomb drop an even chance to
 *    be a Bombchu, and in the single exe only once Link has Bombchus (review H1: the Bombchu count is a shared
 *    progression quantity, so the row must never hand out Bombchus the seed has not given).
 * Fierce Deity Putaway touches only how the held sword is put away; Active Item on B touches the B button's item
 * while one of four items is held, and putting that item away from its own C or D-pad button (it reads the worn mask
 * only to step aside for the Blast, Bremen and Kamaro Masks, and it writes no mask).
 */

#include "SohMenuHostedMm.h"

#include "soh/SohGui/UIWidgets.hpp"

#include "cvar_shared_keys.h" // src/common: the RSBS_CVAR_MM_* defines

namespace SohGui {

/** Quality of Life > Controls, at the end of the section. */
void AddMmControlsRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Manual Jump", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_MANUAL_JUMP)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Makes Z + A jump instead of jump attacking; press B in midair to jump attack. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Prevent Diving Over Water", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_PREVENT_DIVE_OVER_WATER)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Prevents Link from automatically diving over bodies of water. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Infinite Deku Hopping", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_INFINITE_DEKU_HOPPING)
        .Options(UIWidgets::CheckboxOptions().Tooltip("Lets Deku Link hop across water without limit and without "
                                                      "losing speed in the air. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Invert Zora Swim Y Axis", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_INVERT_ZORA_SWIM_Y)
        .Options(UIWidgets::CheckboxOptions().DefaultValue(true).Tooltip(
            "Inverts the Y axis while swimming as Zora with the left stick. Majora's Mask only."));
}

/** Skips & Speed-ups > Animations, right after SoH's Vine/Ladder Climb Speed. */
void AddMmClimbSpeedRow(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Animations", "MM Vine/Ladder Climb Speed %dx", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_PLAYER_CLIMB_SPEED)
        .Options(UIWidgets::IntSliderOptions().Min(1).Max(5).DefaultValue(1).Format("%dx").Tooltip(
            "Multiplies how fast Link climbs vines and ladders. Majora's Mask only."));
}

/** Skips & Speed-ups > Animations, right after SoH's Block Pushing Speed. */
void AddMmPushPullRow(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Animations", "Faster Push/Pull", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_FASTER_PUSH_AND_PULL)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Speeds up pushing and pulling blocks and other objects. Majora's Mask only."));
}

/** Skips & Speed-ups > Animations, at the end of the section. */
void AddMmAnimationsRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Fast Deku Flower Launch", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_FAST_FLOWER_LAUNCH)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Shortens the wait before a Deku Flower launch reaches its full height. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Fierce Deity Putaway", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_FIERCE_DEITY_PUTAWAY)
        .Options(
            UIWidgets::CheckboxOptions().Tooltip("Lets Fierce Deity Link put away his sword. Majora's Mask only."));
}

/** Items > Equipment, at the end of the section. */
void AddMmEquipmentRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Two-Handed Sword Spin Attack", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_EQUIPMENT_TWO_HANDED_SWORD_SPIN_ATTACK)
        .Options(UIWidgets::CheckboxOptions().Tooltip("Enables magic spin attacks with the Fierce Deity's Sword and "
                                                      "the Great Fairy's Sword. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Active Item on B", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_EQUIPMENT_ACTIVE_ITEM_ON_B)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Press B to use the item currently held by Link (Bow, Hookshot, Great Fairy's Sword and Deku Stick). "
            "Second press on item unequips it. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Better Picto Message", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_EQUIPMENT_BETTER_PICTO_MESSAGE)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Names the target, if any, that a pictograph has captured. Majora's Mask only."));
    // 2Ship #1558: Picto Box on C-Up.
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Picto Box on C-Up", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_ITEMS_PICTO_BOX_ON_C_UP)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Press C-Up to activate the Pictograph Box once acquired, without needing to equip it to a C-button. "
            "Majora's Mask only."));
    // 2Ship #1989: an Items row in 2Ship (after its Color Pictograph), here after Better Picto Message.
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Hot Water Never Cools", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_ITEMS_HOT_WATER_NEVER_COOLS)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Prevents Hot Spring Water from cooling down into regular Spring Water. Majora's Mask only."));
}

/** Items > Hookshot, at the end of the section (right after SoH's Targetable Hookshot Reticle). */
void AddMmFasterHookshotRow(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    // 2Ship #1951: a Player row in 2Ship (after its Manual Jump), here with the Hookshot rows.
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Faster Hookshot", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_FASTER_HOOKSHOT)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Speeds up the time it takes for the Hookshot to fire and retract. Majora's Mask only."));
}

/** Items > Boomerang, right after SoH's Instant Boomerang Recall. */
void AddMmFinBoomerangRow(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Boomerang", "Instant Fin Boomerangs Recall", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_ACTIONS_INSTANT_RECALL)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Recalls Zora Link's fin boomerangs at once when B is pressed after they are thrown. Majora's Mask only."));
}

/** Difficulty > Drops, right after SoH's Enable Bombchu Drops. */
void AddMmBombchuDropsRow(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Drops", "MM Enable Bombchu Drops", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_EQUIPMENT_CHU_DROPS)
        .Options(UIWidgets::CheckboxOptions().Tooltip("Converts Bomb drops to Bombchu drops once you have Bombchus: "
                                                      "each Bomb drop then has an even chance to be a Bombchu "
                                                      "instead. Majora's Mask only."));
}

} // namespace SohGui
