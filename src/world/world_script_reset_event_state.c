#include "fft/world.h"
#include "psx/types.h"

void world_script_reset_event_state(void) {
    world_unit_reset_animation_states();
    if (world_thread_is_running_80100164(1) == 0) {
        world_noop_800f6ea8();
        world_noop_800f29d8();
        g_world_camera_speed_curve = 0;
        g_world_camera_position_locked = 0;
    }
}
