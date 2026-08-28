#include "fft/main_sound.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

u32 wldcore_input_check_repeating_directional(u32 buttons);
void wldcore_window_set_layout_states(s32* param);
void wldcore_menu_push_participant_list_level(s32 proposition);

/* Input handler for the proposition list level (type 0x26).
 *
 * The level's first five words are the window/render indices that
 * wldcore_window_set_layout_states consumes. The wrap-around move is spelled as
 * a subtraction of a negative step: that keeps the window's y operand first
 * in the addition, as the target's register assignment requires. */
void wldcore_list_handle_active_propositions_input(wldcore_menu_proposition_list_level_t* level) {
    u32 buttons;
    s32 count;
    s32 window;

    if ((g_wldcore_window_render_records[level->render_index].flags & 0x100) != 0) {
        return;
    }
    buttons = g_wldcore_new_button_presses;
    if ((buttons & PSX_PAD_SELECT) != 0) {
        g_wldcore_window_records[level->cursor_window].sequence = 1;
        window = level->cursor_window;
        g_wldcore_window_records[window].anim_counter = 0;
        g_wldcore_window_records[window].frame_index = 0;
        wldcore_window_set_layout_states(&level->cursor_window);
        wldcore_menu_push_message_level(0x105A, 1);
        return;
    }
    if ((buttons & PSX_PAD_CROSS) != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_wldcore_active_propositions_saved_cursor[0] = 0;
        g_wldcore_window_record_count -= 4;
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 5;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if ((buttons & PSX_PAD_CIRCLE) != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        g_wldcore_active_propositions_saved_cursor[0] = level->cursor;
        g_wldcore_window_record_count -= 4;
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 5;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_menu_push_participant_list_level(level->entries[level->cursor]);
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
