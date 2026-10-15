#include "fft/world.h"
#include "psx/types.h"

/* Formation menu step: either finish the pending job-wheel open (menu 0x15)
 * or advance the wheel scroll position by the frame step, capping at 16. */
void world_formation_update_job_wheel_state(void) {
    s32 step;
    /* The target frame is 0x30 bytes; 0x18 of unreferenced locals. */
    s32 unused[6];

    if (g_world_job_wheel_closing != 0) {
        g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL_OPENING;
        g_world_job_wheel_closing = 0;
        g_world_job_wheel_unit_index = g_unit_selected_index;
    } else {
        if (g_world_job_wheel_opening != 0) {
            step = g_world_job_wheel_open_frame;
            g_world_job_wheel_unit_index = g_unit_selected_index;
            world_formation_build_job_wheel_availability();
            g_world_job_wheel_open_frame
                = step + g_main_input_secondary_repeat_period / world_gfx_get_vsync_mode_or_one();
            if (g_world_job_wheel_open_frame >= 0x11) {
                g_world_job_wheel_open_frame = 0x10;
            }
            g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL_OPENING;
        } else {
            g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL_CLOSING;
        }
    }
    world_formation_layout_job_wheel_portrait();
}
