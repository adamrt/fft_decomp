/* Thread start/resume/suspend/exit, scheduler reset, waits, running test and parameters. */

#include "fft/thread.h"
#include "psx/types.h"

void thread_wait_frames(s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        thread_yield();
    }
}

/* Initialize the slot stack, code and task state; parameters 1-3 are retained. */
void thread_start(s32 thread_id, void (*function)(void)) {
    void* global_pointer = thread_get_current_global_pointer();
    thread_t* thread = &g_threads[thread_id];

    thread->global_pointer = global_pointer;
    thread->stack_pointer = thread->stack_top;
    thread->frame_pointer = thread->stack_top;
    thread->code_pointer = function;
    thread->is_running = 1;
    thread->task_id = THREAD_TASK_NONE;
    thread->function_parameter_4 = 0;
    thread->task_words[0] = 0;
    thread->task_words[1] = 0;
    thread->task_words[2] = 0;
    thread->task_words[3] = 0;
    thread->task_words[4] = 0;
    thread->task_words[5] = 0;
    thread->task_words[6] = 0;
}

void thread_resume(s32 thread_id) {
    g_threads[thread_id].is_running = 1;
}

void thread_suspend(s32 thread_id) {
    g_threads[thread_id].is_running = 0;
}

/* End the current thread: mark its slot idle with no task, then yield. */
void thread_exit_current(void) {
    g_threads[g_thread_current_id].is_running = 0;
    g_threads[g_thread_current_id].task_id = THREAD_TASK_NONE;
    thread_yield();
}

/* Zeroes all 16 thread records (16 * 0x400 bytes), makes thread 0 the current
 * thread and marks it running. */
void thread_reset_scheduler(void) {
    s32* word;
    s32 index;

    index = (THREAD_ARRAY_BYTES / sizeof(s32)) - 1;
    word = (s32*)((u8*)g_threads + THREAD_LAST_WORD_OFFSET);
    do {
        *word = 0;
        index -= 1;
        word -= 1;
    } while (index >= 0);
    g_thread_current_id = THREAD_SLOT_MAIN;
    g_threads[THREAD_SLOT_MAIN].is_running = 1;
}

/* Yield until thread_id's slot is idle. It yields before the first test. */
void thread_wait_until_inactive(s32 thread_id) {
    do {
        thread_yield();
    } while (g_threads[thread_id].is_running != 0);
}

/* Return the running flag of a scheduler slot. */
s32 thread_is_running(s32 thread_id) {
    return g_threads[thread_id].is_running;
}

void thread_set_parameters(s32 thread_id, s32 parameter_1, s32 parameter_2, s32 parameter_3) {
    thread_t* thread = &g_threads[thread_id];
    thread->function_parameter_1 = parameter_1;
    thread->function_parameter_2 = parameter_2;
    thread->function_parameter_3 = parameter_3;
}

void thread_set_four_parameters(s32 slot, s32 first, s32 second, s32 third, s32 fourth) {
    thread_t* thread = &g_threads[slot];
    thread->function_parameter_1 = first;
    thread->function_parameter_2 = second;
    thread->function_parameter_3 = third;
    thread->function_parameter_4 = fourth;
}
