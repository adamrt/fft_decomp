#include "fft/world.h"

/* Keep the VSync wait value and event-speed step synchronized; a zero VSync
 * argument uses the minimum event speed of 1. */
void world_script_set_vsync_mode_and_event_speed(s32 value) {
    g_world_gfx_vsync_mode = value;
    if (value == 0) {
        value = 1;
    }
    world_script_set_event_speed(value);
}
