/* Cooperative thread lifecycle and parameter passing for BATTLE and WORLD.
 * Each scene's frame loop supplies its own context-switch and frame hooks. */

#include "fft/thread.h"
#include "psx/types.h"

/* WORLD has a seventeenth slot for tutorial text; reset clears only sixteen.
 * Unit-view digits and Status numeric frames reuse stack storage in slots 0 and 14. */
thread_t g_thread_contexts[VARIANT_THREAD_TABLE_SLOT_COUNT] = { 0 };
s32 g_thread_current_id;

/* Wait count frames by yielding count times. */
void thread_frames_wait(s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        thread_yield();
    }
}

/* Initialize the slot stack, code and task state; parameters 1-3 are retained. */
void thread_start(s32 thread_id, void (*function)(void)) {
    void* global_pointer = thread_current_global_pointer_get();
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

/* Mark a slot running again; a suspended thread continues where it last yielded. */
void thread_resume(s32 thread_id) {
    g_threads[thread_id].is_running = 1;
}

/* Stop scheduling a slot without resetting it; thread_resume continues it. */
void thread_suspend(s32 thread_id) {
    g_threads[thread_id].is_running = 0;
}

/* End the current thread: mark its slot idle with no task, then yield. */
void thread_current_exit(void) {
    g_threads[g_thread_current_id].is_running = 0;
    g_threads[g_thread_current_id].task_id = THREAD_TASK_NONE;
    thread_yield();
}

/* Zeroes the first 16 thread records (16 * 0x400 bytes), makes thread 0 the current
 * thread and marks it running. */
void thread_scheduler_reset(void) {
    u32* word;
    s32 index = (THREAD_ID_COUNT * sizeof(thread_t) / sizeof(u32)) - 1;

    word = (u32*)(g_threads + THREAD_ID_COUNT) - 1;
    do {
        *word = 0;
        index -= 1;
        word -= 1;
    } while (index >= 0);
    g_thread_current_id = THREAD_SLOT_MAIN;
    g_threads[THREAD_SLOT_MAIN].is_running = 1;
}

/* Yield until thread_id's slot is idle. It yields before the first test. */
void thread_inactive_wait(s32 thread_id) {
    do {
        thread_yield();
    } while (g_threads[thread_id].is_running != 0);
}

/* Return the running flag of a scheduler slot. */
s32 thread_is_running(s32 thread_id) {
    return g_threads[thread_id].is_running;
}

/* Store parameters 1-3 in a slot's record, which the thread reads with
 * thread_current_parameter_1_get..3. thread_start keeps them, and callers also use it to pass new
 * values to a running thread. */
void thread_parameters_set(s32 thread_id, s32 parameter_1, s32 parameter_2, s32 parameter_3) {
    thread_t* thread = &g_threads[thread_id];
    thread->function_parameter_1 = parameter_1;
    thread->function_parameter_2 = parameter_2;
    thread->function_parameter_3 = parameter_3;
}

/* Store parameters 1-4 in a slot's record. thread_start clears parameter 4, so callers set them
 * after starting the thread. */
void thread_four_parameters_set(s32 slot, s32 first, s32 second, s32 third, s32 fourth) {
    thread_t* thread = &g_threads[slot];
    thread->function_parameter_1 = first;
    thread->function_parameter_2 = second;
    thread->function_parameter_3 = third;
    thread->function_parameter_4 = fourth;
}
