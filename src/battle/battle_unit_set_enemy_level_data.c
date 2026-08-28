#include "fft/battle.h"

void battle_unit_set_enemy_level_data(battle_stats_t* unit) {
    u8 lvl;
    if (unit->team_flags & BATTLE_TEAM_MASK) {
        lvl = unit->level;
        g_enemy_level_sum = g_enemy_level_sum + lvl;
        if (g_highest_enemy_level < lvl) {
            g_highest_enemy_level = lvl;
        }
    }
}
