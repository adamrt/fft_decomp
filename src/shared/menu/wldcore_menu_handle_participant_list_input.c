#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Input step of the proposition participant-chooser level.
 *
 * Nothing happens while the render record is hidden (flag 0x100). Once WORLD
 * thread 14's closing animation has finished, the level hands the selected
 * row to wldcore_menu_push_participant_detail_level. Cancel (0x40) tears the level down and returns to the
 * proposition list; confirm (0x20) records the row in g_wldcore_participant_list_saved_cursor and starts
 * thread 14 instead. Both drop one window record, one entry of g_wldcore_window_render_record_count and
 * two render objects. Up/down wrap the cursor and slide the cursor window by
 * one 16-pixel row, and do nothing when there are fewer than two rows.
 *
 * The two directional blocks mirror wldcore_list_handle_active_propositions_input: the up wrap is spelled as
 * a subtraction of a negative step so the window's y stays the first addend,
 * and the down block decrements its own count in place. */
void wldcore_menu_handle_participant_list_input(wldcore_menu_stack_record_t* level) {
    s32 count;

    if (g_wldcore_window_render_records[level->participant_list.render_index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
        g_wldcore_menu_stack_depth--;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_menu_push_participant_detail_level(level->participant_list.proposition, level->participant_list.cursor);
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_wldcore_participant_list_saved_cursor = 0;
        g_wldcore_window_record_count--;
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count -= 2;
        g_wldcore_menu_stack_depth--;
        wldcore_list_open_active_propositions();
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        g_wldcore_window_record_count--;
        g_wldcore_participant_list_saved_cursor = level->participant_list.cursor;
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count -= 2;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
        if (level->participant_list.entry_count >= 2) {
            if (level->participant_list.cursor != 0) {
                g_wldcore_window_records[level->participant_list.cursor_window].position.y -= 0x10;
                level->participant_list.cursor = level->participant_list.cursor - 1;
            } else {
                g_wldcore_window_records[level->participant_list.cursor_window].position.y
                    -= (level->participant_list.entry_count - 1) * -0x10;
                level->participant_list.cursor = level->participant_list.entry_count - 1;
            }
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        }
    }
    if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        count = level->participant_list.entry_count;
        if (count >= 2) {
            count = count - 1;
            if (level->participant_list.cursor < count) {
                g_wldcore_window_records[level->participant_list.cursor_window].position.y += 0x10;
                level->participant_list.cursor = level->participant_list.cursor + 1;
            } else {
                g_wldcore_window_records[level->participant_list.cursor_window].position.y -= count * 0x10;
                level->participant_list.cursor = 0;
            }
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        }
    }
}
