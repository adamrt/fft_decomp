#include "fft/battle.h"
#include "fft/thread.h"
#include "psx/types.h"

void battle_script_sprite_move(void) {
    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_SPRITE_MOVE);
    battle_script_run_sprite_move(battle_thread_get_current_parameter_1(), 0);
    battle_thread_exit_current();
}
