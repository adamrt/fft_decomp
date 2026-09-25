#include "fft/world.h"
#include "psx/types.h"

void world_thread_update_task_state_2(void) {
    s32 running = 0;
    s32 step;
    u8 value;

    world_input_update_controller();
    /* The definition's s8 return conversion would change this call's codegen. */
    step = ((s32 (*)(void))world_menu_get_window_scale_step)();
    if (step >= 1 && step <= 3) {
        world_input_clear_state();
    } else {
        running = world_thread_is_running(1);
        if (running != 0 || g_world_grid_menu_id != 0) {
            world_input_clear_state();
            if (running != 0 && g_world_thread_task_active != 0) {
                value = 1;
                if (g_world_input_blocked_state != 0) {
                    value = 2;
                }
                g_world_input_blocked_state = value;
            } else {
                g_world_input_blocked_state = 0;
            }
        } else if (g_world_input_blocked_state != 0 || world_gfx_get_fade_state() != 0) {
            g_world_input_blocked_state = 0;
            world_input_clear_state();
        }
    }
    if (running == 0) {
        g_world_thread_task_active = 0;
        g_world_sound_release_held_loop = 0;
    }
    if (g_world_input_blocked_state == 1) {
        g_world_menu_sound_effect_id = MAIN_SFX_WINDOW_OPEN;
    }
}
