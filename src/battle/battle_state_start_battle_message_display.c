#include "fft/battle.h"
#include "psx/types.h"

/* Enter BATTLE_GAME_STATE_BATTLE_MESSAGE_DISPLAY.
 *
 * Confirms the ability's targets, then opens the system message window for the
 * first target whose action actually connected.  The message id comes from the
 * ability animation table's third byte; 0x06 (and only 0x06) also plays tune 2,
 * and 0x06/0x2b are the two messages that name both units. */
void battle_state_start_battle_message_display(void) {
    battle_unit_misc_data_t* unit;
    battle_unit_misc_data_t* target;
    s32 message;
    s32 i;

    g_battle_game_state = BATTLE_GAME_STATE_BATTLE_MESSAGE_DISPLAY;
    g_animation_speed = 1;
    g_battle_action_post_action = 0;
    unit = battle_unit_get_casting_misc_data();
    battle_action_build_targets_post_action_message();
    battle_unit_update_attack_result_animation(unit);
    unit->item_ability_display = 0;
    battle_text_set_message_duration_frames(0x3c);
    switch (unit->ability_formula) {
    case 6:
        main_sound_play_tune(2);
        battle_menu_init_system_function(0xa, 0x1838, unit->battle_data->misc_unit_id, 0, 1);
        return;
    case 9:
        main_sound_play_tune(2);
        battle_menu_init_system_function(0xa, 0x1828, unit->battle_data->misc_unit_id, 0, 1);
        return;
    }

    message = 0;
    for (i = 0; i < unit->target_count; i++) {
        target = battle_unit_get_misc_data_by_battle_id(unit->target_list[i]);
        if (target != 0 && target->battle_data != 0 && target->battle_data->action.hit != 0) {
            battle_text_set_message_duration_frames(0x3c);
            message = g_battle_ability_animation_data[unit->sp2_ability_id].text_display;
            if (message == 0) {
                break;
            }
            if (message == 6) {
                /* Clobbers $v0 so the constant 6 is reloaded, not kept in $s4 across the call. */
                __asm__ volatile("" : : : "$2");
                main_sound_play_tune(2);
            }
            break;
        }
    }

    if (message == 6 || message == 0x2b) {
        battle_menu_init_system_function(
            0xa, message | 0x1800, unit->battle_data->misc_unit_id, target->battle_data->misc_unit_id, 1);
    } else if (message != 0) {
        battle_menu_init_system_function(0xa, message | 0x1800, unit->battle_data->misc_unit_id, 0, 1);
    } else {
        battle_menu_init_system_function(0xa, 0, unit->battle_data->misc_unit_id, 0, 0);
    }
}
