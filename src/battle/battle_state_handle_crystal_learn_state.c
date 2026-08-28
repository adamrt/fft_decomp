#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/battle_camera.h"
#include "fft/battle_move.h"
#include "fft/battle_state.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"

/*
 * Post-action state tick: apply the selected ability's outcome, then once the
 * caster's wait counter passes 0x1f, retire one pending move-find result.
 */
void battle_state_handle_crystal_learn_state(void) {
    s32* ability;
    battle_unit_misc_data_t* source;
    battle_unit_misc_data_t* casting;
    u16 counter;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    ability = battle_menu_get_selected_ability_address();
    source = battle_unit_get_source_misc_data();
    casting = battle_unit_get_casting_misc_data();
    switch (*ability) {
    case 2:
        battle_unit_learn_from_crystal(source->battle_data, 2);
        g_battle_action_post_action = 1;
        battle_action_finalize_attack_and_flag_reactions(source->battle_data->misc_unit_id);
        battle_gfx_prepare_post_action_display(source);
        battle_unit_update_display_by_misc_id(source->unit_id);
        break;
    case 0:
    case 1:
    case 4:
        battle_unit_learn_from_crystal(source->battle_data, *ability);
        g_battle_action_post_action = 1;
        break;
    case 7:
    case 8:
    case 0xff:
        g_battle_action_post_action = 1;
        break;
    }

    if (g_battle_action_post_action == 0) {
        return;
    }
    if (g_battle_state_animation_continue_check != 0) {
        return;
    }
    counter = casting->state_frame_counter;
    casting->state_frame_counter = counter + 1;
    if (counter < 0x1f) {
        return;
    }
    if (source->numeric_display_active != 0) {
        return;
    }
    if (casting->numeric_display_active != 0) {
        return;
    }

    if (g_battle_move_find_result & 1) {
        if (battle_unit_get_crystal_or_treasure_at_map_coords(source->map_x, source->map_y, source->map_z) != 0) {
            return;
        }
        g_battle_move_find_result &= ~1;
        battle_ai_init_selected_action();
    } else if (g_battle_move_find_result & 2) {
        g_battle_action_post_action_display_phase += 1;
        if (g_battle_action_post_action_display_phase >= 2) {
            g_battle_move_find_result &= ~2;
        }
    } else if (g_battle_move_find_result & 4) {
        if ((source->animation_countdown != 0) && ((u32)(source->encoded_animation >> 1) >= 0xc)) {
            return;
        }
        source->item_ability_display = 0;
        g_battle_move_find_result &= ~4;
    } else if (g_battle_move_find_result & 8) {
        g_battle_move_find_result &= ~8;
        battle_ai_init_selected_action();
        battle_unit_update_display_by_misc_id(source->unit_id);
    } else if (g_battle_move_find_result & 0x10) {
        g_battle_move_find_result &= ~0x10;
    } else if (g_battle_move_find_result & 0x200) {
        g_battle_move_find_result &= ~0x200;
    } else if (g_battle_move_find_result & 0x400) {
        g_battle_move_find_result &= ~0x400;
    } else {
        return;
    }
    battle_move_start_next_post_movement_step();
}
