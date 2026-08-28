#include "fft/battle.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_script_map_darkness(void) {
    u8* parameter;

    world_thread_set_current_task_id(NATIVE_THREAD_TASK_MAP_DARKNESS);
    parameter = world_thread_get_current_parameter_1();
    battle_map_start_ambient_light_blend(
        parameter[0], parameter[4], (s8)parameter[1], (s8)parameter[2], (s8)parameter[3]);
    world_thread_wait_frames(g_world_map_darkness_wait_frames[parameter[4]]);
    world_thread_exit_current();
}
