/* Thread accessors for the current global pointer, parameters 1 to 3 and running checks. */

#include "fft/thread.h"
#include "psx/types.h"

/* Capture $gp for a new native thread's saved context. */
void* thread_current_global_pointer_get(void) {
    void* global_pointer;

    __asm__("move %0, $gp" : "=r"(global_pointer));
    return global_pointer;
}

/* The original entries below use $at address loads and fixed $t0/$t1 scratch
 * registers; WORLD's build adds the record offset with `add`, BATTLE's with
 * `addu`. */
__asm__(".set noat");
#ifdef FFT_WORLD
#    define THREAD_ADD_RECORD "add"
#else
#    define THREAD_ADD_RECORD "addu"
#endif

/* Return parameter 1 of the current thread's record. */
s32 thread_current_parameter_1_get(void) {
    register s32 thread_offset __asm__("$8");
    register thread_t* threads __asm__("$9");
    register thread_t* thread __asm__("$8");

    THREAD_LOAD_GLOBAL(thread_offset, g_thread_current_id);
    __asm__("nop\nsll $8,$8,10" : "=r"(thread_offset) : "0"(thread_offset));
    THREAD_LOAD_GLOBAL(threads, g_threads);
    __asm__("nop\n" THREAD_ADD_RECORD " %0,%1,%2" : "=r"(thread) : "0"(thread_offset), "r"(threads));
    return thread->function_parameter_1;
}

/* Return parameter 2 of the current thread's record. */
s32 thread_current_parameter_2_get(void) {
    register s32 thread_offset __asm__("$8");
    register thread_t* threads __asm__("$9");
    register thread_t* thread __asm__("$8");

    THREAD_LOAD_GLOBAL(thread_offset, g_thread_current_id);
    __asm__("nop\nsll $8,$8,10" : "=r"(thread_offset) : "0"(thread_offset));
    THREAD_LOAD_GLOBAL(threads, g_threads);
    __asm__("nop\n" THREAD_ADD_RECORD " %0,%1,%2" : "=r"(thread) : "0"(thread_offset), "r"(threads));
    return thread->function_parameter_2;
}

/* Return parameter 3 of the current thread's record. */
s32 thread_current_parameter_3_get(void) {
    register s32 thread_offset __asm__("$8");
    register thread_t* threads __asm__("$9");
    register thread_t* thread __asm__("$8");

    THREAD_LOAD_GLOBAL(thread_offset, g_thread_current_id);
    __asm__("nop\nsll $8,$8,10" : "=r"(thread_offset) : "0"(thread_offset));
    THREAD_LOAD_GLOBAL(threads, g_threads);
    __asm__("nop\n" THREAD_ADD_RECORD " %0,%1,%2" : "=r"(thread) : "0"(thread_offset), "r"(threads));
    return thread->function_parameter_3;
}

/* Read the preceding slot; slot 0 has no wraparound. */
s32 thread_is_previous_running(void) {
    register s32 thread_offset __asm__("$8");
    register thread_t* threads __asm__("$9");
    register thread_t* thread __asm__("$8");

    THREAD_LOAD_GLOBAL(thread_offset, g_thread_current_id);
    __asm__("nop\naddi $8,$8,-1\nsll $8,$8,10" : "=r"(thread_offset) : "0"(thread_offset));
    THREAD_LOAD_GLOBAL(threads, g_threads);
    __asm__("nop\n" THREAD_ADD_RECORD " %0,%1,%2" : "=r"(thread) : "0"(thread_offset), "r"(threads));
    return thread->is_running;
}

/* Read the slot's running flag using the assembled entry's register sequence. */
s32 thread_is_running_asm(s32 thread_id) {
    register s32 thread_offset __asm__("$8");
    register thread_t* threads __asm__("$9");
    register thread_t* thread __asm__("$8");

    thread_offset = thread_id << 10;
    THREAD_LOAD_GLOBAL(threads, g_threads);
    __asm__("nop\n" THREAD_ADD_RECORD " %0,%1,%2" : "=r"(thread) : "0"(thread_offset), "r"(threads));
    return thread->is_running;
}
