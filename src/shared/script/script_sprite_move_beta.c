#include "fft/script.h"
#include "fft/thread.h"

/* Thread body of SpriteMoveBeta: run the sprite move with the last operand taken
 * as a speed. */
void script_sprite_move_beta(void) {
    thread_set_current_task_id(THREAD_TASK_SPRITE_MOVE);
    script_run_sprite_move((const event_sprite_move_parameters_t*)thread_get_current_parameter_1_variant(), 1);
    thread_exit_current();
}
