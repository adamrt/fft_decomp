#include "fft/battle.h"
#include "psx/types.h"

void battle_target_set_coordinates_for_ability(battle_stats_t* acting, u8* out) {
    battle_stats_t* target;

    out[0x12] = 0;
    if (g_current_ability.random_fire_flag != 0) {
        out[0x1C] = g_current_ability.target_x;
        out[0x1D] = g_current_ability.target_y;
        out[0x1E] = g_current_ability.target_elevation;
    } else if (acting->action_target_kind == BATTLE_ACTION_TARGET_UNIT) {
        target = &g_battle_unit_stats[acting->action_target_id];
        out[0x1C] = target->x;
        out[0x1D] = target->position.bits.y;
        out[0x1E] = target->position.raw >> 15;
    } else {
        out[0x1C] = acting->action_target_x;
        out[0x1D] = acting->action_target_y;
        out[0x1E] = acting->action_target_elevation;
    }
}
