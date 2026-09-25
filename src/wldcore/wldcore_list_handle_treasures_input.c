#include "fft/wldcore.h"
#include "psx/pad.h"

/* Treasure-list level input step (cursor slot 8); twin of wldcore_list_handle_unexplored_lands_input.
 * Cancel (0x40) and confirm (0x20) wait for file loading, start thread 12's
 * closing animation and record the choice in result; 0x100 on a non-empty
 * list opens help message 0x101f. Once thread 12 finishes, the level closes
 * and a confirmed choice passes the selected entry's value to 0x8007efa0.
 *
 * The target passes the level to 0x8007e178, which reads no argument. */
void wldcore_list_handle_treasures_input(wldcore_menu_panel_level_t* level) {
    if ((g_main_system_flags & 4) && world_thread_is_running(0xC) == 0) {
        g_wldcore_menu_ordering_table_offset = 1;
        g_wldcore_menu_result = 0;
        g_main_system_flags ^= 4;
        g_wldcore_menu_stack_depth--;
        if (level->result != 0) {
            wldcore_list_store_cursor_state(8);
            wldcore_menu_push_treasure_detail_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & 0x7FF);
            return;
        }
        wldcore_list_clear_cursor_state(8);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if ((g_wldcore_new_button_presses & PSX_PAD_CROSS) && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        world_thread_set_parameters(0xC, 0, 0, 1);
        g_main_system_flags |= 4;
    } else if ((g_wldcore_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_unexplored_lands_clear_result(level);
        wldcore_menu_push_message_level(0x101F, 1);
    } else if ((g_wldcore_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        world_thread_set_parameters(0xC, 0, 0, 1);
        g_main_system_flags |= 4;
    }
}
