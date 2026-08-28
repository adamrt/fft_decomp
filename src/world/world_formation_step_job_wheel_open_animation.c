#include "fft/world.h"
#include "psx/types.h"

void world_formation_step_job_wheel_open_animation(void) {
    s32 pos;

    if (g_world_job_wheel_opening == 0) {
        world_formation_build_job_wheel_availability();
    }
    pos = g_world_job_wheel_open_frame * 0x140 + ONE;
    world_formation_draw_job_wheel_sprites(((20 - g_world_job_wheel_open_frame) << 10) / 20 + 0xC00, pos, pos);
    g_world_job_wheel_open_frame--;
    if (g_world_job_wheel_open_frame == 0) {
        g_world_job_wheel_opening = 0;
        g_world_formation_current_menu = 3;
    }
}
