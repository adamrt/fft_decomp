#include "fft/world.h"
#include "psx/types.h"

/* Event instruction 0x41 EarthquakeStart thread: every parameter[1] frames add
 * parameter[0] to g_world_camera_shake_z_offset, every parameter[3] frames add parameter[2]. */
void world_script_earthquake_start(void) {
    s32 first_frames;
    s32 second_frames;
    u8* parameter;

    world_thread_set_current_task_id(NATIVE_THREAD_TASK_EARTHQUAKE);
    first_frames = 0;
    parameter = world_thread_get_current_parameter_1();
    second_frames = 0;
    for (;;) {
        first_frames++;
        world_thread_yield();
        second_frames++;
        if (first_frames == parameter[1]) {
            first_frames = 0;
            g_world_camera_shake_z_offset += parameter[0];
        }
        if (second_frames == parameter[3]) {
            g_world_camera_shake_z_offset += parameter[2];
            second_frames = 0;
        }
    }
}
