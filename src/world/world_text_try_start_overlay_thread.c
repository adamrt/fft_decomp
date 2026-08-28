#include "fft/battle_runtime.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_text_try_start_overlay_thread(void) {
    if (world_thread_is_running_80100164(8) || world_thread_is_running_80100164(3)) {
        if (g_world_menu_current_id != 0xA) {
            g_world_menu_building_thread_started = 1;
        }
    }
    if (g_world_thread_change_cooldown == 0 && g_world_menu_transition_active == 0 && battle_camera_is_active() == 0
        && g_world_companion_overlay_state == 0 && world_thread_is_running_80100164(3) == 0) {
        g_world_text_overlay_active = 1;
        world_thread_start(3, world_text_run_overlay_thread);
        world_thread_set_parameters(3, 0, 0, 0);
    }
}
