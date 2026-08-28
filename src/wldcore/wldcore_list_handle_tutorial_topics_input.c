#include "fft/main_file.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

void wldcore_menu_push_tutorial_topic_text_level(s32 value);
s32 world_thread_is_running(s32 thread_id);
void world_thread_set_parameters(s32 thread_id, s32 a, s32 b, s32 c);

/* Tutorial-topic list level input step (cursor slot 12); twin of
 * wldcore_list_handle_treasures_input with help message 0x108d. A confirmed topic's value is
 * passed to 0x80084578, which opens the tutorial text window. */
void wldcore_list_handle_tutorial_topics_input(wldcore_menu_panel_level_t* level) {
    if ((g_main_system_flags & 4) && world_thread_is_running(0xC) == 0) {
        g_wldcore_menu_result = 0;
        g_wldcore_menu_ordering_table_offset = 1;
        g_main_system_flags ^= 4;
        g_wldcore_menu_stack_depth--;
        if (level->result != 0) {
            wldcore_list_store_cursor_state(0xC);
            wldcore_menu_push_tutorial_topic_text_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & 0x7FF);
            return;
        }
        wldcore_list_clear_cursor_state(0xC);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if ((g_wldcore_new_button_presses & PSX_PAD_CROSS) && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        world_thread_set_parameters(0xC, 0, 0, 1);
        g_main_system_flags |= 4;
    } else if ((g_wldcore_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_tutorial_topics_clear_result(level);
        wldcore_menu_push_message_level(0x108D, 1);
    } else if ((g_wldcore_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        world_thread_set_parameters(0xC, 0, 0, 1);
        g_main_system_flags |= 4;
    }
}
