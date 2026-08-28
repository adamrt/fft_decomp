#include "fft/battle_runtime.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_menu_display_triangle_selection(void) {
    world_thread_set_current_task_id(NATIVE_THREAD_TASK_MENU_SOUND_DELAY);
    while (battle_camera_is_active() != 0 || g_world_menu_transition_timeout != 0) {
        world_thread_yield();
    }
    world_script_pulse_tutorial_wait_value(0xFE);
    g_world_menu_hide_numeric_values = 0;
    world_menu_run_icon_selection_loop();
}
