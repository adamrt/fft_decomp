#include "fft/wldcore.h"
#include "psx/pad.h"

/* Yes/no list-level input step. Cancel (0x40) or confirm (0x20) starts
 * thread 12's closing animation and records the choice in result; 0x100
 * with a non-empty list opens help message 0x105e. Once thread 12 finishes,
 * the level closes and a confirmed choice passes the selected entry's value
 * (low 11 bits of g_wldcore_list_entry_values) to 0x80072618, otherwise the bar menu resumes.
 *
 * The target passes the level to 0x80072404 and 0x800723f0 even though
 * neither reads an argument; their local prototypes here take it. */
void wldcore_list_handle_rumors_input(wldcore_menu_panel_level_t* level) {
    if ((g_main_system_flags & 4) && world_thread_is_running(0xC) == 0) {
        wldcore_list_rumors_close_level(level);
        if (level->result != 0) {
            wldcore_list_store_cursor_state(2);
            wldcore_bar_push_rumor_detail_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & 0x7FF);
            return;
        }
        wldcore_list_clear_cursor_state(2);
        g_main_system_flags &= ~0x800;
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if ((g_wldcore_new_button_presses & PSX_PAD_CROSS) && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        world_thread_set_parameters(0xC, 0, 0, 1);
        g_main_system_flags |= 4;
    } else if ((g_wldcore_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_rumors_clear_result(level);
        wldcore_menu_push_message_level(0x105E, 1);
        return;
    } else if ((g_wldcore_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        world_thread_set_parameters(0xC, 0, 0, 1);
        g_main_system_flags |= 4;
    }
}
