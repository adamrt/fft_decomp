#include "fft/thread.h"

/* BATTLE twin of world_thread_start; same context layout (fft/thread.h). */
void battle_thread_start(s32 thread_id, void (*function)(void)) {
    void* global_pointer = battle_thread_get_current_global_pointer();
    battle_thread_t* thread = &g_battle_threads[thread_id];

    thread->global_pointer = global_pointer;
    thread->stack_pointer = thread->stack_top;
    thread->frame_pointer = thread->stack_top;
    thread->code_pointer = function;
    thread->is_running = 1;
    thread->task_id = 0;
    thread->function_parameter_4 = 0;
    thread->task_words[0] = 0;
    thread->task_words[1] = 0;
    thread->task_words[2] = 0;
    thread->task_words[3] = 0;
    thread->task_words[4] = 0;
    thread->task_words[5] = 0;
    thread->task_words[6] = 0;
}
