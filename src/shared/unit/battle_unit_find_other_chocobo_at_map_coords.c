#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"

/* Find the other Chocobo renderer record occupying a map tile.
 *
 * Mounting logic excludes the acting and casting records when multiple
 * Chocobo records share the coordinates.
 */
battle_unit_t* battle_unit_find_other_chocobo_at_map_coords(s32 x, s32 y, s32 z) {
    battle_unit_t* unit;
    battle_unit_t** cursor;
    s32 count;
    s32 id;
    s32 spritesheet_id;
    battle_unit_t* list[BATTLE_UNIT_RUNTIME_SLOT_COUNT];
    battle_unit_t* result;
    s32 index;

    unit = g_battle_unit_list_head;
    count = 0;
    if (unit != 0) {
        spritesheet_id = BATTLE_SPRITESHEET_ID_CHOCOBO;
        cursor = list;
        for (; unit != 0; unit = unit->previous) {
            if (unit->map_x == x && unit->map_y == y && unit->map_z == z && unit->spritesheet_id == spritesheet_id) {
                *cursor = unit;
                cursor += 1;
                count += 1;
            }
        }
    }
    if (count != 0) {
        for (index = 0; index < count; index++) {
            result = list[index];
            id = result->runtime_id;
            if (id != g_battle_unit_source_runtime_id && id != g_battle_casting_runtime_id) {
                return result;
            }
        }
    }
    return 0;
}
