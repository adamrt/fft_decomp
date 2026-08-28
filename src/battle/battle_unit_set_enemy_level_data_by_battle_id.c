#include "fft/battle.h"
#include "psx/types.h"

s32 battle_unit_set_enemy_level_data_by_battle_id(u32 battle_id) {
    battle_stats_t* stats = &g_battle_unit_stats[battle_id];

    if (stats->entd_slot != BATTLE_ENTD_SLOT_NONE)
        return 0;
    if (stats->existence != 0)
        return -1;
    stats->entd_slot = battle_id;
    stats->existence = BATTLE_UNIT_EXISTENCE_ACTIVE;
    if (stats->team_flags & BATTLE_TEAM_MASK) {
        battle_unit_set_enemy_level_data(stats);
    }
    return 0;
}
