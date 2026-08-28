#include "fft/world.h"
#include "psx/types.h"

/* Starts menu thread 8 on world_menu_display_triangle_selection with the g_world_menu_system_entries menu
 * data when threads 7 and 8 are idle and g_world_menu_overlay_state is clear. */
void world_menu_start_mini_menu_display_thread(void) {
    if (world_thread_is_running_80100164(8) != 0) {
        return;
    }
    if (world_thread_is_running_80100164(7) != 0) {
        return;
    }
    if (*(u16*)&g_world_menu_overlay_state != 0) {
        return;
    }
    g_world_menu_current_id = 10;
    g_world_menu_thread_menu_data = g_world_menu_system_entries;
    world_thread_start(8, world_menu_display_triangle_selection);
    world_thread_set_parameters(8, (s32)&g_world_menu_thread_menu_data[10], 0, 0);
    g_world_thread_contexts[8].task_id = NATIVE_THREAD_TASK_MENU_SOUND_DELAY;
}
