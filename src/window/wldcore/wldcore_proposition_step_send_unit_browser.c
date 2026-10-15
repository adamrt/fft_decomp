#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

void wldcore_proposition_step_send_unit_browser(wldcore_menu_send_unit_level_t* level) {
    u32 buttons;
    s32 selected;
    s32 direction;
    s16* menu_selected_index;

    if (level->mode == WLDCORE_SEND_UNIT_MODE_STATUS_OPEN) {
        if (thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) != 0) {
            return;
        }
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
            return;
        }
        g_wldcore_window_records[level->left_arrow].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->right_arrow].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        level->mode = WLDCORE_SEND_UNIT_MODE_STATUS;
        menu_selected_index = &g_wldcore_window_panel_render_state.selected_index;
        wldcore_start_core_background_threads(g_wldcore_list_entry_values[*menu_selected_index] & TEXT_ID_ENTRY_MASK);
        level->slot = *menu_selected_index;
    } else if (level->mode == WLDCORE_SEND_UNIT_MODE_STATUS) {
        buttons = g_wldcore_input_new_button_presses;
        if (buttons & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            g_wldcore_window_records[level->left_arrow].flags |= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[level->right_arrow].flags |= WORLD_RENDER_RECORD_HIDDEN;
            level->mode = WLDCORE_SEND_UNIT_MODE_STATUS_CLOSE;
            thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
            thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, 0, 0, 1);
            thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
            return;
        }
        if (buttons & PSX_PAD_SELECT) {
            g_wldcore_window_records[level->left_arrow].palette = g_wldcore_window_records[level->right_arrow].palette
                = 0xA;
            wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            wldcore_menu_push_layout_window_level();
            return;
        }
        if (!(g_input_current_buttons & (PSX_PAD_L1 | PSX_PAD_R1))) {
            g_wldcore_window_records[level->left_arrow].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON;
            g_wldcore_window_records[level->right_arrow].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON;
            g_wldcore_window_records[level->left_arrow].frame_index
                = g_wldcore_window_records[level->left_arrow].anim_counter
                = g_wldcore_window_records[level->right_arrow].frame_index
                = g_wldcore_window_records[level->right_arrow].anim_counter = 0;
        }
        if (wldcore_input_update_repeat(PSX_PAD_R1) != 0
            && g_wldcore_window_records[level->left_arrow].sequence != WLDCORE_ANIM_SEQUENCE_L1_BUTTON_PRESS) {
            g_wldcore_window_records[level->right_arrow].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON_PRESS;
            g_wldcore_window_records[level->right_arrow].frame_index
                = g_wldcore_window_records[level->right_arrow].anim_counter = 0;
            if (level->unit_count == 1) {
                /* Shared tail: duplicating the sound call per arm is only
                 * partly cross-jumped and leaves an extra copy. */
                goto done;
            }
            selected = level->slot + 1;
            if (selected >= level->unit_count) {
                level->slot = 0;
            } else {
                level->slot = selected;
            }
            direction = 1;
        } else {
            if (wldcore_input_update_repeat(PSX_PAD_L1) == 0
                || g_wldcore_window_records[level->right_arrow].sequence == WLDCORE_ANIM_SEQUENCE_R1_BUTTON_PRESS) {
                return;
            }
            g_wldcore_window_records[level->left_arrow].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON_PRESS;
            g_wldcore_window_records[level->left_arrow].frame_index
                = g_wldcore_window_records[level->left_arrow].anim_counter = 0;
            if (level->unit_count == 1) {
                goto done;
            }
            if (level->slot != 0) {
                level->slot--;
            } else {
                level->slot = level->unit_count - 1;
            }
            direction = 2;
        }
        g_wldcore_thread8_params.redraw_request = direction;
        wldcore_unit_build_status_panel_data(g_wldcore_list_entry_values[level->slot] & TEXT_ID_ENTRY_MASK);
        g_wldcore_threadc_params.redraw_request = 1;
        g_wldcore_thread9_params.redraw_request = 1;
    done:
        wldcore_sound_play_effect(MAIN_SFX_PAGE_SWITCH);
    } else if (level->mode == WLDCORE_SEND_UNIT_MODE_STATUS_CLOSE) {
        if (thread_is_running(THREAD_SCREEN_SLOT_UNIT_STATUS) != 0) {
            return;
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_NUMERIC_EDITOR) != 0) {
            return;
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) != 0) {
            return;
        }
        level->mode = WLDCORE_SEND_UNIT_MODE_SELECT;
        g_main_system_flags &= ~(MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE | MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS);
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_CHOOSE_UNITS, 0);
        thread_start(WLDCORE_THREAD_SLOT_WINDOW_PANEL, world_screen_build_at_list);
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, (s32)&g_wldcore_window_panel_render_state, 0, 0);
        g_wldcore_window_records[level->list_frame.window_index].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->list_frame.render_index].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->frame_window_a].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->frame_window_b].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->preview_render].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->list_render].flags ^= WORLD_RENDER_RECORD_HIDDEN;
    }
}
