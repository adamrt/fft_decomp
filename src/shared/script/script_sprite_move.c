#include "fft/script.h"
#include "fft/thread.h"

/* Thread body of SpriteMove: run the sprite move with the last operand taken as a
 * frame count. */
void script_sprite_move(void) {
    thread_set_current_task_id(THREAD_TASK_SPRITE_MOVE);
    script_run_sprite_move((const event_sprite_move_parameters_t*)battle_thread_get_current_parameter_1(), 0);
    thread_exit_current();
}
