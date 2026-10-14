#include "fft/battle.h"
#include "psx/types.h"

/* Enter BATTLE_GAME_STATE_BATTLE_MESSAGE_DISPLAY.
 *
 * Confirms the ability's targets, then opens the system message window for the
 * first target whose action actually connected.  The message id comes from the
 * ability animation table's third byte; 0x06 (and only 0x06) also plays tune 2,
 * and 0x06/0x2b are the two messages that name both units. */
void battle_state_start_battle_message_display(void) {
    battle_unit_t* unit;
    battle_unit_t* target;
    battle_ability_animation_data_t* animation;
    s32 message;
    s32 i;

    g_battle_game_state = BATTLE_GAME_STATE_BATTLE_MESSAGE_DISPLAY;
    g_main_system_animation_speed = 1;
    g_action_post_action = 0;
    unit = battle_unit_get_casting();
    action_build_targets_post_action_message();
    battle_unit_update_attack_result_animation(unit);
    unit->item_display_active = 0;
    text_set_message_duration_frames(0x3c);
    switch (unit->strike.post_action_message) {
    case BATTLE_POST_ACTION_MESSAGE_POACHED:
        main_sound_play_tune(MAIN_SOUND_TUNE_ITEM_FOUND);
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0x1838, unit->stats->battle_id, 0, 1);
        return;
    case BATTLE_POST_ACTION_MESSAGE_TRAINED:
        main_sound_play_tune(MAIN_SOUND_TUNE_ITEM_FOUND);
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0x1828, unit->stats->battle_id, 0, 1);
        return;
    }

    message = 0;
    for (i = 0; i < unit->strike.target_count; i++) {
        target = battle_unit_get_by_battle_id(unit->strike.target_list[i]);
        if (target != 0 && target->stats != 0 && target->stats->action.hit != 0) {
            text_set_message_duration_frames(0x3c);
            /* Taking the row first keeps this loop above loop.c's size limit
             * for hoisting the constant 6 into a saved register. */
            animation = &g_battle_ability_animation_data[unit->sp2_ability_id];
            message = animation->text_display;
            if (message == 0) {
                break;
            }
            if (message == 6) {
                main_sound_play_tune(MAIN_SOUND_TUNE_ITEM_FOUND);
            }
            break;
        }
    }

    if (message == 6 || message == 0x2b) {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, message | 0x1800, unit->stats->battle_id,
            target->stats->battle_id, 1);
    } else if (message != 0) {
        menu_init_system_function(
            BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, message | 0x1800, unit->stats->battle_id, 0, 1);
    } else {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    }
}
