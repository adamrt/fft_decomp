#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Provisional: proposition participant-chooser level pushed over the
 * proposition list (0x80085760). One cursor window rides a render record
 * listing entry_count participant rows. */
typedef struct wldcore_menu_participant_list_level {
    s32 cursor_window; /* 0x00 */
    u8 unknown_04[4];
    s32 render_index; /* 0x08 */
    u8 unknown_0c[4];
    s32 proposition; /* 0x10; index into g_main_active_propositions */
    u8 unknown_14[8];
    s32 cursor; /* 0x1c; selected row, 0..entry_count-1 */
    u8 unknown_20[0x10];
    s32 entry_count; /* 0x30 */
} wldcore_menu_participant_list_level_t;

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
void wldcore_menu_handle_participant_list_input(wldcore_menu_participant_list_level_t* level) {
    s32 count;

    if (g_wldcore_window_render_records[level->render_index].flags & 0x100) {
        return;
    }
    if ((g_main_system_flags & 4) && world_thread_is_running(0xE) == 0) {
        g_wldcore_menu_stack_depth--;
        g_main_system_flags ^= 4;
        wldcore_menu_push_participant_detail_level(level->proposition, level->cursor);
        return;
    }
    if (g_wldcore_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_wldcore_participant_list_saved_cursor = 0;
        g_wldcore_window_record_count--;
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count -= 2;
        g_wldcore_menu_stack_depth--;
        wldcore_list_open_active_propositions();
        return;
    }
    if (g_wldcore_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        g_wldcore_window_record_count--;
        g_wldcore_participant_list_saved_cursor = level->cursor;
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count -= 2;
        world_thread_set_parameters(0xE, 0, -1, 0);
        g_main_system_flags |= 4;
        return;
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_UP) != 0) {
        if (level->entry_count >= 2) {
            if (level->cursor != 0) {
                g_wldcore_window_records[level->cursor_window].y -= 0x10;
                level->cursor = level->cursor - 1;
            } else {
                g_wldcore_window_records[level->cursor_window].y -= (level->entry_count - 1) * -0x10;
                level->cursor = level->entry_count - 1;
            }
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        }
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_DOWN) != 0) {
        count = level->entry_count;
        if (count >= 2) {
            count = count - 1;
            if (level->cursor < count) {
                g_wldcore_window_records[level->cursor_window].y += 0x10;
                level->cursor = level->cursor + 1;
            } else {
                g_wldcore_window_records[level->cursor_window].y -= count * 0x10;
                level->cursor = 0;
            }
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        }
    }
}
