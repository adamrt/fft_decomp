#include "fft/world.h"
#include "psx/types.h"

void world_script_sprite_move_beta(void) {
    world_thread_set_current_task_id(NATIVE_THREAD_TASK_SPRITE_MOVE);
    world_script_run_sprite_move(world_thread_get_current_parameter_1(), 1);
    world_thread_exit_current();
}
