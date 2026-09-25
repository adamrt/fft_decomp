#include "fft/wldcore.h"
#include "psx/pad.h"

/* List level input step for cursor slot 16 (the list opened by 0x80087b94):
 * twin of wldcore_list_handle_unexplored_lands_input with help message 0x105b and no file-loading gate.
 * Closing without a selection re-flags the list window's content render
 * record (0x100) before returning to the bar menu; a confirmed entry goes
 * to 0x80087fa4. */
void wldcore_list_handle_brave_story_propositions_input(wldcore_menu_panel_level_t* level) {
    s32 depth;

    if ((g_main_system_flags & 4) && world_thread_is_running(0xC) == 0) {
        depth = g_wldcore_menu_stack_depth;
        g_wldcore_menu_result = 0;
        g_wldcore_menu_ordering_table_offset = 1;
        g_main_system_flags ^= 4;
        g_wldcore_menu_stack_depth = depth - 1;
        if (level->result != 0) {
            wldcore_list_store_cursor_state(0x10);
            wldcore_menu_push_proposition_attempt_detail_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & 0x7FF);
            return;
        }
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.content_render].flags
            |= 0x100;
        wldcore_list_clear_cursor_state(0x10);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (g_wldcore_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        world_thread_set_parameters(0xC, 0, 0, 1);
        g_main_system_flags |= 4;
    } else if ((g_wldcore_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_menu_push_message_level(0x105B, 1);
    } else if ((g_wldcore_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        world_thread_set_parameters(0xC, 0, 0, 1);
        g_main_system_flags |= 4;
    }
}
