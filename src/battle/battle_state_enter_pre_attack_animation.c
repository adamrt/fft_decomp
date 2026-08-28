#include "fft/battle.h"
#include "psx/types.h"

void battle_state_enter_pre_attack_animation(void) {
    battle_unit_misc_data_t* unit;
    u16 ability;

    battle_target_set_tile_background_color(0, 2);
    g_battle_game_state = BATTLE_GAME_STATE_PRE_ATTACK_ANIMATION;
    unit = battle_unit_get_source_misc_data();
    ability = unit->used_ability_id;
    unit->state_frame_counter = 0;
    if (g_battle_ability_animation_data[ability].charge_animation_set_id != 0) {
        battle_unit_start_ability_charge_animation(unit);
    } else {
        /* facing is read signed (lh) here. */
        battle_unit_store_animation_facing(2, *(s16*)&unit->facing, unit);
    }
    battle_action_clear_at_list_active();
}
