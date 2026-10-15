#include "fft/battle.h"
#include "fft/battle_unit.h"

/* Counts a unit of any team but blue into the enemy level statistics: its level
 * is added to g_battle_enemy_level_sum and raises g_battle_highest_enemy_level. */
void battle_unit_set_enemy_level_data(unit_stats_t* unit) {
    u8 lvl;
    if (unit->team_flags & BATTLE_TEAM_MASK) {
        lvl = unit->level;
        g_battle_enemy_level_sum = g_battle_enemy_level_sum + lvl;
        if (g_battle_highest_enemy_level < lvl) {
            g_battle_highest_enemy_level = lvl;
        }
    }
}
