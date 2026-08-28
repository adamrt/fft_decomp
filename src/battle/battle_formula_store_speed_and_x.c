#include "fft/battle.h"

void battle_formula_store_speed_and_x(void) {
    s32 speed = g_battle_action_attacker->attributes[UNIT_ATTRIBUTE_SPEED];
    s32 x = g_current_ability.range_data.x;

    g_current_ability.ya = x;
    g_current_ability.xa = speed;
}
