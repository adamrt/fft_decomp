#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void battle_formula_heal_mp_z_times_ten(void) {
    s32 index;
    s32 amount;
    battle_action_data_t* action;

    index = g_current_ability.used_item_id;
    action = g_battle_action_target_data;
    amount = g_main_item_secondary_data[index].z;
    action->attack_type = BATTLE_ACTION_TYPE_MP_HEALING;
    action->mp_healing = amount * 10;
}
