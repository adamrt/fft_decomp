#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_script_play_effect_thread(void) {
    world_thread_set_current_task_id(NATIVE_THREAD_TASK_PLAY_EFFECT);
    world_script_play_effect_and_wait();
    world_thread_exit_current();
}
