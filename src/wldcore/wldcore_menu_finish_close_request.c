#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_menu_finish_close_request(wldcore_menu_message_level_t* level) {
    if (g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth - 1] == WLDCORE_MENU_LEVEL_BRAVE_STORY
        || g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth - 1] == WLDCORE_MENU_LEVEL_PROPOSITION_CATEGORY) {
        g_wldcore_displayed_numeric_value = g_main_system_play_time_hours;
        g_wldcore_displayed_numeric_value_secondary = g_main_system_play_time_minutes;
        g_wldcore_displayed_numeric_value_tertiary = g_main_system_play_time_seconds;
    }
    if (world_thread_is_running(2) == 0) {
        g_wldcore_menu_result = level->saved_menu_result;
        g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth - 1;
        wldcore_menu_dispatch_resume_handler();
        if (level->reload_text != 0) {
            world_text_restore_section_pointers();
        }
    }
}
