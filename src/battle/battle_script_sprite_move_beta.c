#include "fft/battle.h"
#include "psx/types.h"

void battle_script_sprite_move_beta(void) {
    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_SPRITE_MOVE);
    battle_script_run_sprite_move(battle_thread_get_current_parameter_1(), 1);
    battle_thread_exit_current();
}
