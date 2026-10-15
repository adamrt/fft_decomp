#include "fft/battle_action.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Finds another unit standing on tile (x, y, z): one with battle stats in an
 * occupied slot, other than self and, when self is mounted or ridden, other than
 * its mount partner.
 *
 * Among several, the source unit (g_battle_unit_source_runtime_id) is preferred,
 * then the first in list order. Returns 0 for none. The candidate array holds 16
 * entries and is filled without a bound check. */
battle_unit_t* battle_unit_find_overlapping_at_tile(battle_unit_t* self, s32 x, s32 y, s32 z) {
    battle_unit_t* list[BATTLE_UNIT_RUNTIME_SLOT_COUNT];
    battle_unit_t* unit;
    unit_stats_t* stats;
    s32 count;
    s32 i;

    count = 0;
    for (unit = g_battle_unit_list_head; unit != 0; unit = unit->previous) {
        if (unit != self && unit->map_x == x && unit->map_y == y && unit->map_z == z) {
            stats = unit->stats;
            if (stats != 0 && stats->entd_slot != BATTLE_ENTD_SLOT_NONE) {
                if (self->mount_state == BATTLE_UNIT_MOUNT_STATE_NONE
                    || unit->runtime_id != self->mount_partner_runtime_id) {
                    list[count] = unit;
                    count += 1;
                }
            }
        }
    }
    if (count != 0) {
        for (i = 0; i < count; i++) {
            if (list[i]->runtime_id == g_battle_unit_source_runtime_id) {
                return list[i];
            }
        }
        return list[0];
    }
    return 0;
}
