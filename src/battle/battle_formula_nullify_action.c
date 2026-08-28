#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_nullify_action(void) {
    battle_action_data_t* target_data;
    battle_action_data_t* target_data_reload;

    g_battle_action_target_data->hit = 0;
    target_data = g_battle_action_target_data;
    target_data->miss_type = BATTLE_ACTION_MISS_TYPE_NULLIFIED;
    target_data_reload = g_battle_action_target_data;
    *(s16*)&target_data->hp_damage = 0;
    /* Halfword stores clear the byte pairs at 0x2a and 0x10. */
    target_data->attack_accuracy = 0;
    target_data->special_effect = 0;
    target_data_reload->attack_type = 0;
    battle_formula_clear_nullify_flags();
    battle_action_clear_knockback_flag();
}
