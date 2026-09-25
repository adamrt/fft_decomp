#include "fft/battle.h"

void battle_state_handle_learn_ability_on_hit_state(void) {
    s32* selected_ability_address;
    s32 selected_ability;
    s32 counter;
    s32 skip_animation;
    s32 hit_count;
    battle_unit_misc_data_t* casting_unit;
    battle_unit_misc_data_t* target_unit;
    u8 compiler_stack_pad[8];

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    selected_ability_address = battle_menu_get_selected_ability_address();
    casting_unit = battle_unit_get_casting_misc_data();
    selected_ability = *selected_ability_address;
    switch (selected_ability) {
    case 7:
        if (g_battle_action_post_action_display_phase == 2) {
            target_unit = battle_unit_get_misc_data_by_battle_id(*(u16*)&g_battle_action_post_action_unit_id);
            battle_unit_learn_used_ability(target_unit->battle_data);
        }
    case 8:
    case 0xff:
        g_battle_action_post_action_display_phase += 1;
        if (g_battle_action_post_action_display_phase >= 3) {
            g_battle_action_post_action = 1;
        } else {
            battle_action_init_learn_ability_on_hit();
        }
        break;
    }
    if (g_battle_action_post_action != 0 && g_battle_state_animation_continue_check == 0) {
        counter = 0;
        if (casting_unit->numeric_display_active == 0) {
            hit_count = casting_unit->target_count;
            skip_animation = 0;
            if (hit_count > 0) {
                do {
                    target_unit = battle_unit_get_misc_data_by_battle_id(casting_unit->target_list[counter]);
                    if (target_unit != 0 && target_unit->numeric_display_active != 0) {
                        skip_animation = 1;
                        break;
                    }
                    hit_count = casting_unit->target_count;
                    counter += 1;
                } while (counter < hit_count);
            }
            if (skip_animation == 0) {
                casting_unit = battle_unit_get_source_misc_data();
                battle_unit_call_set_animation_based_on_status(casting_unit);
                battle_unit_update_post_command_animation_display(casting_unit);
            }
        }
    }
}
