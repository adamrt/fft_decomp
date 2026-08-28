#include "fft/battle.h"

/* Find the other Chocobo renderer record occupying a map tile.
 *
 * Mounting logic excludes the acting and casting records when multiple
 * Chocobo records share the coordinates.
 */
battle_unit_misc_data_t* battle_unit_find_other_chocobo_at_map_coords(s32 x, s32 y, s32 z) {
    battle_unit_misc_data_t* misc;
    battle_unit_misc_data_t** cursor;
    s32 count;
    s32 id;
    s32 spritesheet_id;
    battle_unit_misc_data_t* list[16];
    battle_unit_misc_data_t* result;
    s32 index;

    misc = g_battle_unit_misc_list_head;
    count = 0;
    if (misc != 0) {
        spritesheet_id = BATTLE_SPRITESHEET_ID_CHOCOBO;
        cursor = list;
        do {
            if (misc->map_x == x && misc->map_y == y && misc->map_z == z && misc->spritesheet_id == spritesheet_id) {
                *cursor = misc;
                cursor += 1;
                count += 1;
            }
            misc = misc->previous;
        } while (misc != 0);
    }
    if (count != 0) {
        for (index = 0; index < count; index++) {
            result = list[index];
            id = result->unit_id;
            if (id != g_battle_casting_misc_id && id != g_battle_casting_unit_id) {
                return result;
            }
        }
    }
    return 0;
}
