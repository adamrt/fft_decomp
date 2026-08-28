#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

void wldcore_list_open_active_propositions(void);
void wldcore_window_begin_tint_transition(wldcore_window_tint_request_t* request);
u32 wldcore_input_check_repeating_directional(u32 buttons);
void wldcore_list_open_brave_story_propositions(void);

/* Input step for the brave-story top-level list window.
 *
 * Refreshes the displayed play-time copies each frame, then, while the side
 * render record is not pending (flag 0x100): button 0x100 opens the help
 * message for the highlighted row (0x105a + entry), button 0x40 pops the
 * level, button 0x20 dispatches on the row value (0 opens the brave-story
 * submenu at wldcore_list_open_active_propositions, 1 the proposition list), and up/down wrap the
 * cursor, moving the cursor window by 16 pixels a row. The wrap-around move
 * is spelled as a subtraction of a negative step to keep the window's y as
 * the first addition operand. */
void wldcore_menu_handle_proposition_category_input(wldcore_menu_list_window_level_t* level) {
    s32 window;
    s32 entry;
    s32 count;
    u32 buttons;

    g_wldcore_displayed_numeric_value = g_main_system_play_time_hours;
    g_wldcore_displayed_numeric_value_secondary = g_main_system_play_time_minutes;
    g_wldcore_displayed_numeric_value_tertiary = g_main_system_play_time_seconds;

    if ((g_wldcore_window_render_records[level->side_window].flags & 0x100) != 0) {
        return;
    }
    buttons = g_wldcore_new_button_presses;
    if ((buttons & PSX_PAD_SELECT) != 0) {
        g_wldcore_window_records[level->main_window].palette = 0xA;
        g_wldcore_window_render_records[level->side_window].palette = 2;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 2]
                                            .list_window.content_render]
            .palette = 2;
        g_wldcore_window_records[level->main_window].sequence = 1;
        window = level->main_window;
        g_wldcore_window_records[window].anim_counter = 0;
        g_wldcore_window_records[window].frame_index = 0;
        wldcore_menu_push_message_level(level->entries[level->selected_entry] + 0x105A, 1);
        return;
    }
    if ((buttons & PSX_PAD_CROSS) != 0 && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_wldcore_proposition_category_saved_cursor[0] = 0;
        g_wldcore_window_record_count -= 1;
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 2;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if ((g_wldcore_new_button_presses & PSX_PAD_CIRCLE) != 0 && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        g_wldcore_proposition_category_saved_cursor[0] = level->selected_entry;
        entry = level->entries[level->selected_entry];
        switch (entry) {
        case 0:
            wldcore_window_begin_tint_transition((wldcore_window_tint_request_t*)level);
            g_wldcore_window_records[level->main_window].sequence = 1;
            wldcore_list_open_active_propositions();
            break;
        case 1:
            wldcore_window_begin_tint_transition((wldcore_window_tint_request_t*)level);
            g_wldcore_window_records[level->main_window].sequence = entry;
            wldcore_list_open_brave_story_propositions();
            break;
        }
        return;
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_UP) != 0) {
        if (level->entry_count >= 2) {
            if (level->selected_entry != 0) {
                g_wldcore_window_records[level->main_window].y -= 0x10;
                level->selected_entry = level->selected_entry - 1;
            } else {
                g_wldcore_window_records[level->main_window].y -= (level->entry_count - 1) * -0x10;
                level->selected_entry = level->entry_count - 1;
            }
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        }
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_DOWN) != 0) {
        count = level->entry_count;
        if (count >= 2) {
            count = count - 1;
            if (level->selected_entry < count) {
                g_wldcore_window_records[level->main_window].y += 0x10;
                level->selected_entry = level->selected_entry + 1;
            } else {
                g_wldcore_window_records[level->main_window].y -= count * 0x10;
                level->selected_entry = 0;
            }
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        }
    }
}
