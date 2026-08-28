#include "fft/battle.h"
#include "fft/thread.h"
#include "psx/types.h"

void battle_script_play_effect_thread(void) {
    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_PLAY_EFFECT);
    battle_effect_play_and_wait_for_animation();
    battle_thread_exit_current();
}
