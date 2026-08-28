#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void battle_action_set_item_throw_stone_ability_display(void) {
    battle_unit_misc_data_t* unit;
    s32 ability;

    g_battle_game_state = BATTLE_GAME_STATE_ACTION_EXECUTE;
    unit = battle_unit_get_casting_misc_data();
    ability = unit->used_ability_id;
    unit->state_frame_counter = 0;
    battle_target_gather_x_y_data_for_attacks(unit);

    if ((ability == 0) || (ability == 0x200)) {
        g_animation_speed = 1;
        battle_action_clear_at_list_active();
        return;
    }
    if (battle_effect_load_ability(ability) != 0) {
        g_animation_speed = 1;
        battle_action_clear_at_list_active();
        return;
    }
    if ((ability == ABILITY_ID_BASIC_SKILL_ACCUMULATE)
        || ((u32)(ability - ABILITY_ID_CHARGE_FIRST) < (ABILITY_ID_MATH_FIRST - ABILITY_ID_CHARGE_FIRST))) {
        if (battle_effect_is_item_ability(ability) != 0) {
            g_animation_speed = 1;
            battle_action_clear_at_list_active();
            return;
        }
        battle_effect_play();
        g_animation_speed = 1;
        battle_action_clear_at_list_active();
        return;
    }
    if ((battle_effect_is_item_ability(ability) == 0) || ((unit->encoded_animation >> 1) == 0x39)) {
        battle_effect_play();
    }
    main_sound_pause_tracked_sfx();
    g_animation_speed = 2;
    battle_action_clear_at_list_active();
}
