#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Records the location window request and starts the menu thread-group supervisor on thread 2, unless a
 * menu screen is already opening. */
void world_menu_start_thread_group(s32 parameter, s32 location_id, s32 window_x, s32 window_y) {
    if (g_world_menu_screen_open_request == 0) {
        if (g_world_menu_location_id != location_id) {
            g_threads[12].task_id = THREAD_TASK_NONE;
        }
        g_world_menu_location_text_id = location_id + TEXT_ID_LOCATION_NAME_BASE;
        g_world_menu_thread_group_parameter = parameter;
        g_world_menu_location_id = location_id;
        g_world_menu_location_window_x = window_x;
        g_world_menu_location_window_y = window_y;
        g_world_menu_default_entries[2].text_id = location_id + TEXT_ID_LOCATION_NAME_BASE;
        g_world_menu_default_entries[2].window.x = window_x;
        g_world_menu_default_entries[2].window.y = window_y;
        thread_start(WORLD_THREAD_SLOT_MENU_GROUP, world_menu_thread_group_supervisor_thread);
        thread_set_parameters(WORLD_THREAD_SLOT_MENU_GROUP, parameter, 0, 0);
    }
}
