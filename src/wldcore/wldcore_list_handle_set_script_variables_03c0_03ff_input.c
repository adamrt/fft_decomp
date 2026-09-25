#include "fft/wldcore.h"
#include "psx/pad.h"

/* Script-variable list level input step (cursor slot 11) for the panel opened
 * by 0x8008343c; twin of wldcore_list_handle_unexplored_lands_input with help message 0x1056. Cancelling
 * restores the parent list window's content render record (flag 0x100); a
 * confirmed entry's value is passed to 0x8008389c. */
void wldcore_list_handle_set_script_variables_03c0_03ff_input(wldcore_menu_panel_level_t* level) {
    if ((g_main_system_flags & 4) && world_thread_is_running(0xC) == 0) {
        g_wldcore_menu_result = 0;
        g_wldcore_menu_ordering_table_offset = 1;
        g_main_system_flags ^= 4;
        g_wldcore_menu_stack_depth--;
        if (level->result != 0) {
            wldcore_list_store_cursor_state(0xB);
            wldcore_menu_push_variable_detail_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & 0x7FF);
            return;
        }
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                            .list_window.content_render]
            .flags |= 0x100;
        wldcore_list_clear_cursor_state(0xB);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (g_wldcore_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        world_thread_set_parameters(0xC, 0, 0, 1);
        g_main_system_flags |= 4;
    } else if ((g_wldcore_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_script_variables_clear_result(level);
        wldcore_menu_push_message_level(0x1056, 1);
    } else if ((g_wldcore_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        world_thread_set_parameters(0xC, 0, 0, 1);
        g_main_system_flags |= 4;
    }
}
