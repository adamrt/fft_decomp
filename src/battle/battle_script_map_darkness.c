#include "fft/battle.h"
#include "fft/option.h"
#include "fft/thread.h"
#include "psx/types.h"

void battle_script_map_darkness(void) {
    u8* parameters;

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_MAP_DARKNESS);
    parameters = (u8*)battle_thread_get_current_parameter_1();
    battle_map_start_ambient_light_blend(
        parameters[0], parameters[4], (s8)parameters[1], (s8)parameters[2], (s8)parameters[3]);
    battle_thread_wait_frames(g_battle_script_map_darkness_durations[parameters[4]]);
    battle_thread_exit_current();
}
