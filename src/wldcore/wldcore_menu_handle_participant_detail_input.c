#include "fft/wldcore.h"
#include "psx/gpu.h"
#include "psx/pad.h"

void wldcore_menu_handle_participant_detail_input(wldcore_menu_participant_level_t* level) {
    RECT rect;
    u32 buttons;
    s32 count;
    s32 direction;
    s32 window;
    s32 participant;

    if ((g_main_system_flags & 4) && world_thread_is_running(8) == 0) {
        rect.x = 0x200;
        rect.y = 0x100;
        rect.w = 0x40;
        rect.h = 0x100;
        g_main_system_flags &= 0xFFFDFFFB;
        g_wldcore_menu_stack_depth--;
        MoveImage(&rect, 0x180, 0);
        DrawSync(0);
        g_world_text_substitution_values[0] = g_main_active_propositions[level->proposition].proposition_id | 0xD000;
        world_thread_start(0xE, world_text_message_box_thread);
        world_thread_set_parameters(0xE, 0x19, 0xB849, 0);
        wldcore_menu_push_participant_list_level(level->proposition);
        return;
    }
    buttons = g_wldcore_new_button_presses;
    if (buttons & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_main_system_flags |= 4;
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_object_count -= 2;
        world_thread_set_parameters(8, 0, 0, 1);
        world_thread_set_parameters(0xC, 0, 0, 1);
        world_thread_set_parameters(9, 0, 0, 1);
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        g_wldcore_window_records[level->left_window].palette = g_wldcore_window_records[level->right_window].palette
            = 0xA;
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        wldcore_menu_push_layout_window_level();
        return;
    }
    if (!(g_wldcore_current_button_input & (PSX_PAD_L1 | PSX_PAD_R1))) {
        g_wldcore_window_records[level->left_window].sequence = 0x66;
        g_wldcore_window_records[level->right_window].sequence = 0x68;
        g_wldcore_window_records[level->left_window].frame_index
            = g_wldcore_window_records[level->left_window].anim_counter
            = g_wldcore_window_records[level->right_window].frame_index
            = g_wldcore_window_records[level->right_window].anim_counter = 0;
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_R1) != 0
        && g_wldcore_window_records[level->left_window].sequence != 0x67) {
        g_wldcore_window_records[level->right_window].sequence = 0x69;
        window = level->right_window;
        g_wldcore_window_records[window].anim_counter = 0;
        g_wldcore_window_records[window].frame_index = 0;
        count = g_main_active_propositions[level->proposition].participant_count;
        if (count == 1) {
            /* Shared tail: both single-participant cases branch to the closing
             * sound effect; nesting the redraw under `count != 1` changes the
             * register allocation. */
            goto done;
        }
        participant = level->participant + 1;
        if (participant >= count) {
            level->participant = 0;
        } else {
            level->participant = participant;
        }
        direction = 1;
    } else {
        if (wldcore_input_check_repeating_directional(PSX_PAD_L1) == 0
            || g_wldcore_window_records[level->right_window].sequence == 0x69) {
            return;
        }
        g_wldcore_window_records[level->left_window].sequence = 0x67;
        window = level->left_window;
        g_wldcore_window_records[window].anim_counter = 0;
        g_wldcore_window_records[window].frame_index = 0;
        if (g_main_active_propositions[level->proposition].participant_count == 1) {
            goto done;
        }
        if (level->participant != 0) {
            level->participant = level->participant - 1;
        } else {
            level->participant = g_main_active_propositions[level->proposition].participant_count - 1;
        }
        direction = 2;
    }
    g_wldcore_thread8_params.redraw_request = direction;
    wldcore_unit_build_status_panel_data(
        g_main_active_propositions[level->proposition].participant_indices[level->participant]);
    g_wldcore_threadc_params.redraw_request = 1;
    g_wldcore_thread9_params.redraw_request = 1;
done:
    wldcore_sound_play_effect(MAIN_SFX_PAGE_SWITCH);
}
