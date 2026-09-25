#include "fft/wldcore.h"
#include "psx/pad.h"

/* List level input step for cursor slot 20; twin of wldcore_list_handle_tutorial_topics_input with help
 * message 0x108f, handing a confirmed entry's value to 0x80092618. */
void wldcore_list_handle_script_flags_01a4_01bb_input(wldcore_menu_panel_level_t* level) {
    if ((g_main_system_flags & 4) && world_thread_is_running(0xC) == 0) {
        g_wldcore_menu_result = 0;
        g_wldcore_menu_ordering_table_offset = 1;
        g_main_system_flags ^= 4;
        g_wldcore_menu_stack_depth--;
        if (level->result != 0) {
            wldcore_list_store_cursor_state(0x14);
            wldcore_menu_push_script_flag_01a4_detail_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & 0x7FF);
            return;
        }
        wldcore_list_clear_cursor_state(0x14);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if ((g_wldcore_new_button_presses & PSX_PAD_CROSS) && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        world_thread_set_parameters(0xC, 0, 0, 1);
        g_main_system_flags |= 4;
    } else if ((g_wldcore_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_script_flags_clear_result(level);
        wldcore_menu_push_message_level(0x108F, 1);
    } else if ((g_wldcore_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        world_thread_set_parameters(0xC, 0, 0, 1);
        g_main_system_flags |= 4;
    }
}
