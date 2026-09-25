#include "fft/battle.h"
#include "psx/types.h"

/* Test (save == 0) or set the found flag of one of a map's four move-find item tiles.
 *
 * Each flag byte holds two maps, four tiles each; the test returns 1 while the item is
 * still unfound. */
s32 battle_map_calculate_move_find_item_flag(s32 map_id, s32 tile_index, s32 save) {
    s32 byte_index;
    s32 bit;

    byte_index = map_id / 2;
    bit = 0x80 >> (((map_id & 1) * 4) + tile_index);
    if (save == 0) {
        return (g_main_item_location_flags[byte_index] & bit) == 0;
    }
    g_main_item_location_flags[byte_index] = (u8)bit | g_main_item_location_flags[byte_index];
    return 1;
}
