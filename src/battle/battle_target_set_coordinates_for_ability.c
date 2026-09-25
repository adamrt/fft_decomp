#include "fft/battle.h"
#include "psx/types.h"

void battle_target_set_coordinates_for_ability(battle_stats_t* acting, battle_strike_work_t* out) {
    battle_stats_t* target;

    out->animate_on_miss_flag = 0;
    if (g_current_ability.random_fire_flag != 0) {
        out->target_new_x = g_current_ability.target_x;
        out->target_new_y = g_current_ability.target_y;
        out->target_new_map_level = g_current_ability.target_elevation;
    } else if (acting->action_target_kind == BATTLE_ACTION_TARGET_UNIT) {
        target = &g_battle_unit_stats[acting->action_target_id];
        out->target_new_x = target->x;
        out->target_new_y = target->position.bits.y;
        out->target_new_map_level = target->position.raw >> 15;
    } else {
        out->target_new_x = acting->action_target_x;
        out->target_new_y = acting->action_target_y;
        out->target_new_map_level = acting->action_target_elevation;
    }
}
