#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/pad.h"

/*
 * Input step of the proposition participant detail level (the unit status
 * panels of one unit sent on a job).
 *
 * Circle or Cross asks the three panel threads to close and blocks input;
 * once the status banner thread has ended the level pops, the saved screen
 * strip is moved back, line 0xB849 is shown with the proposition's name and
 * the participant list level is pushed again. Select opens the layout window.
 * R1 and L1 step to the next or previous participant with wrap-around and
 * request a redraw of the panels (banner direction 1 or 2); with a single
 * participant they only play the page sound. The L1/R1 prompts show their
 * pressed sequence while held, and one button is ignored while the other's
 * pressed sequence is showing.
 */
void wldcore_menu_handle_participant_detail_input(wldcore_menu_participant_level_t* level) {
    RECT rect;
    u32 buttons;
    s32 count;
    s32 direction;
    s32 participant;

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(THREAD_SCREEN_SLOT_UNIT_STATUS) == 0) {
        rect.x = 0x200;
        rect.y = 0x100;
        rect.w = 0x40;
        rect.h = 0x100;
        g_main_system_flags &= ~(MAIN_SYSTEM_FLAG_INPUT_BLOCKED | MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS);
        g_wldcore_menu_stack_depth--;
        MoveImage(&rect, 0x180, 0);
        DrawSync(0);
        g_text_substitution_values[0]
            = g_main_active_propositions[level->proposition].proposition_id | TEXT_ID_PROPOSITION_NAME_BASE;
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
        wldcore_menu_push_participant_list_level(level->proposition);
        return;
    }
    buttons = g_wldcore_input_new_button_presses;
    if (buttons & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_object_count -= 2;
        thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
        thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, 0, 0, 1);
        thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        g_wldcore_window_records[level->left_window].palette = g_wldcore_window_records[level->right_window].palette
            = 0xA;
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        wldcore_menu_push_layout_window_level();
        return;
    }
    if (!(g_input_current_buttons & (PSX_PAD_L1 | PSX_PAD_R1))) {
        g_wldcore_window_records[level->left_window].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON;
        g_wldcore_window_records[level->right_window].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON;
        g_wldcore_window_records[level->left_window].frame_index
            = g_wldcore_window_records[level->left_window].anim_counter
            = g_wldcore_window_records[level->right_window].frame_index
            = g_wldcore_window_records[level->right_window].anim_counter = 0;
    }
    if (wldcore_input_update_repeat(PSX_PAD_R1) != 0
        && g_wldcore_window_records[level->left_window].sequence != WLDCORE_ANIM_SEQUENCE_L1_BUTTON_PRESS) {
        g_wldcore_window_records[level->right_window].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON_PRESS;
        g_wldcore_window_records[level->right_window].frame_index
            = g_wldcore_window_records[level->right_window].anim_counter = 0;
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
        if (wldcore_input_update_repeat(PSX_PAD_L1) == 0
            || g_wldcore_window_records[level->right_window].sequence == WLDCORE_ANIM_SEQUENCE_R1_BUTTON_PRESS) {
            return;
        }
        g_wldcore_window_records[level->left_window].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON_PRESS;
        g_wldcore_window_records[level->left_window].frame_index
            = g_wldcore_window_records[level->left_window].anim_counter = 0;
        if (g_main_active_propositions[level->proposition].participant_count == 1) {
            goto done;
        }
        if (level->participant != 0) {
            level->participant--;
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
