#include "fft/battle_unit.h"
#include "psx/types.h"

/* Puts an off-field unit on the field: the slot becomes occupied, its existence
 * active, and a unit of any team but blue is counted into the enemy level
 * statistics.
 *
 * Returns 0, also for a unit already present, or -1 when the slot is empty in any
 * state other than off-field. */
s32 battle_unit_set_enemy_level_data_by_battle_id(u32 battle_id) {
    unit_stats_t* stats = &g_battle_unit_stats[battle_id];

    if (stats->entd_slot != BATTLE_ENTD_SLOT_NONE)
        return 0;
    if (stats->existence != BATTLE_UNIT_EXISTENCE_OFF_FIELD)
        return -1;
    stats->entd_slot = battle_id;
    stats->existence = BATTLE_UNIT_EXISTENCE_ACTIVE;
    if (stats->team_flags & BATTLE_TEAM_MASK) {
        battle_unit_set_enemy_level_data(stats);
    }
    return 0;
}
