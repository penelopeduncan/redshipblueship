#pragma once

// #1741: until #1740 the Cosmetics Editor copied controller port 0's D-pad mappings onto the four custom ocarina-note
// buttons (F4, A4, B4, D5). Once anything saved a note button's mapping id list, the copied D-pad ids stayed in that
// list and came back at every start. A copy shares its id, and so its ButtonMappings.<id> settings record, with the
// D-pad's own mapping, so removing the copy from a note row erased the D-pad's binding as well.
//
// Every real mapping's id embeds the bitmask of the button it was made for ("P<port>-B<bitmask>-<kind>", libultraship
// ButtonMappingFactory and the *ToButtonMapping GetButtonMappingId formats), so an id on a note list whose B field is
// some other bitmask cannot be one of that note's own bindings.

#include <cstddef>
#include <cstdint>
#include <string>

// Pure: returns idList ("id,id,...,") without every id whose "P<digits>-B<digits>-" bitmask field is not ownBitmask.
// Ids that are not in that shape are kept, and empty tokens are left out. *droppedOut (when not null) receives the
// number of ids dropped.
std::string SohInput_StripForeignBitmaskMappingIds(const std::string& idList, uint32_t ownBitmask, size_t* droppedOut);

// Runs SohInput_StripForeignBitmaskMappingIds over the four custom ocarina-note buttons' saved mapping id lists on
// every port, rewriting a list only when it dropped an id (cleared when nothing is left). Writes those CVars only:
// never Save(), never a ButtonMappings.<id> record, never another button's list. Must run before the first
// ControlDeck::Init (osContInit), which loads the lists into memory. Returns true when it rewrote any list.
bool SohInput_DropForeignOcarinaNoteMappingIds();
