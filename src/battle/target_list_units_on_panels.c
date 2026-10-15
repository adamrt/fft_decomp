#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Fill `list` (BATTLE_UNIT_SLOT_COUNT bytes, 0xff padded) with the ids of present units whose
 * tile carries `MAP_TILE_FLAG_TARGETED`, and return the count.
 * target_is_unit_untargetable_and_store_tile returns nonzero for units
 * that cannot be counted and writes the unit's tile index. When nearest-first
 * ordering is enabled the list is insertion-sorted by Manhattan distance from
 * `origin`.
 */
s32 target_list_units_on_panels(u8* list, const unit_stats_t* origin) {
    /* A byte flag test retains the original compound-condition scheduling. */
    s32 distance[BATTLE_UNIT_SLOT_COUNT];
    s32 tile;
    s32 count;
    s32 i;
    s32 j;
    s32 k;
    s32 dx;
    s32 dy;
    s32 dist;
    s32 x;
    s32 y;
    unit_stats_t* unit;

    count = 0;
    x = origin->x;
    y = origin->position.y;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && target_is_unit_untargetable_and_store_tile(i, &tile) == 0
            && ((*(u8*)&g_map_tile_data[tile].ceiling_depth_and_marks) & MAP_TILE_FLAG_TARGETED)) {
            if (g_battle_sort_targets_nearest_first != 0) {
                dx = x - unit->x;
                if (dx < 0) {
                    dx = -dx;
                }
                dy = y - unit->position.y;
                if (dy < 0) {
                    dy = -dy;
                }
                dist = dx + dy;
                for (j = 0; j < count; j++) {
                    if (dist < distance[j]) {
                        break;
                    }
                }
                for (k = BATTLE_UNIT_SLOT_COUNT - 1; j < k; k--) {
                    list[k] = list[k - 1];
                    distance[k] = distance[k - 1];
                }
                distance[j] = dist;
            } else {
                j = count;
            }
            list[j] = i;
            count++;
        }
    }
    for (j = count; j < BATTLE_UNIT_SLOT_COUNT; j++) {
        list[j] = 0xFF;
    }
    return count;
}
