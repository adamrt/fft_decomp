#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Fills the caller's panel entry buffer with the menu entry ids offered at a
 * world map location and returns how many were written; the two callers store
 * the result as the panel's entry_count and pass the buffer as entries.
 *
 * Location 0x16 offers a run of consecutive entries 0xb8ed + i, one per unit
 * counted by script variable 0x65, plus one. Every other location offers the
 * three fixed entries 0xb85d-0xb85f when its state record's is_town byte is 1, and
 * appends a fourth entry 0xb860 when script variable 0x90 is set and the
 * location is Dorter, Warjilis or Zarghidas (the fur shop).
 */
s32 wldcore_map_build_location_menu_entries(s32 location_id, s32* out) {
    s32 count;
    s32 i;

    if (location_id == WORLD_LOCATION_DEEP_DUNGEON) {
        count = script_get_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_LIST_LENGTH) + 1;
        for (i = 0; i < count; i++) {
            out[i] = WLDCORE_TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST + i;
        }
        return count;
    }

    count = 0;
    if (g_wldcore_location_records[location_id].is_town == 1) {
        count = 3;
        out[0] = WLDCORE_TEXT_ID_BAR;
        out[1] = WLDCORE_TEXT_ID_SHOP;
        out[2] = WLDCORE_TEXT_ID_SOLDIER_OFFICE;
        if (script_get_variable(EVENT_SCRIPT_VAR_FUR_SHOP_ENABLED) != 0) {
            if ((location_id == WORLD_LOCATION_DORTER) || (location_id == WORLD_LOCATION_WARJILIS)
                || (location_id == WORLD_LOCATION_ZARGHIDAS)) {
                out[count] = WLDCORE_TEXT_ID_FUR_SHOP;
                count++;
            }
        }
    }
    return count;
}
