#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* LearnAbilityOnHit game-state handler: steps through the post-action reports
 * (level up, job level up, learn on hit).
 *
 * Command 7 on the learn-on-hit report teaches the used ability to the unit found
 * for it. Commands 7, 8 and 0xff all advance to the next report, and after the
 * third the action is marked finished. From then on, once no animation is pending
 * and neither the casting unit nor any of its targets still shows a numeric
 * display, the source unit returns to its status animation and the after-command
 * state begins. */
void battle_state_handle_learn_ability_on_hit_state(void) {
    ai_command_t* selected_command_address;
    s32 selected_command;
    s32 counter;
    s32 skip_animation;
    s32 hit_count;
    battle_unit_t* casting_unit;
    battle_unit_t* target_unit;
    u8 compiler_stack_pad[8];

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command_address = menu_get_selected_command_address();
    casting_unit = battle_unit_get_casting();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case 7:
        if (g_action_post_action_display_phase == BATTLE_POST_ACTION_DISPLAY_LEARN_ON_HIT) {
            target_unit = battle_unit_get_by_battle_id(g_action_learn_on_hit_unit_id);
            battle_unit_learn_used_ability(target_unit->stats);
        }
    case 8:
    case 0xff:
        g_action_post_action_display_phase += 1;
        if (g_action_post_action_display_phase >= 3) {
            g_action_post_action = 1;
        } else {
            action_init_learn_ability_on_hit();
        }
        break;
    }
    if (g_action_post_action != 0 && g_battle_state_animation_continue_check == 0) {
        counter = 0;
        if (casting_unit->numeric_display.active == 0) {
            hit_count = casting_unit->strike.target_count;
            skip_animation = 0;
            if (hit_count > 0) {
                do {
                    target_unit = battle_unit_get_by_battle_id(casting_unit->strike.target_list[counter]);
                    if (target_unit != 0 && target_unit->numeric_display.active != 0) {
                        skip_animation = 1;
                        break;
                    }
                    hit_count = casting_unit->strike.target_count;
                    counter += 1;
                } while (counter < hit_count);
            }
            if (skip_animation == 0) {
                casting_unit = battle_unit_get_source();
                battle_unit_call_set_animation_based_on_status(casting_unit);
                battle_unit_update_post_command_animation_display(casting_unit);
            }
        }
    }
}
