/* Thread parameters and globals, one object of the library BATTLE and WORLD link in the same order; cuts inside the
 * library are chosen by topic. */

#include "fft/thread.h"

/* Capture $gp for a new native thread's saved context. */
void* thread_get_current_global_pointer(void) {
    void* global_pointer;

    __asm__("move %0, $gp" : "=r"(global_pointer));
    return global_pointer;
}

#ifdef FFT_BATTLE
/* The original entry uses $at address loads and fixed $t0/$t1 scratch registers. */
__asm__(".set noat");

/* Return parameter 1 of the current thread's record. */
s32 thread_get_current_parameter_1_variant(void) {
    register s32 thread_offset __asm__("$8");
    register thread_t* threads __asm__("$9");
    register thread_t* thread __asm__("$8");

    THREAD_LOAD_GLOBAL(thread_offset, g_thread_current_id);
    __asm__("nop\nsll $8,$8,10" : "=r"(thread_offset) : "0"(thread_offset));
    THREAD_LOAD_GLOBAL(threads, g_threads);
    __asm__("nop\naddu %0,%1,%2" : "=r"(thread) : "0"(thread_offset), "r"(threads));
    return thread->function_parameter_1;
}
#endif

#ifdef FFT_WORLD
/* The original entry uses $at address loads and fixed $t0/$t1 scratch registers. */
__asm__(".set noat");

/* Return parameter 1 of the current thread's record. */
s32 thread_get_current_parameter_1_variant(void) {
    register s32 thread_offset __asm__("$8");
    register thread_t* threads __asm__("$9");
    register thread_t* thread __asm__("$8");

    THREAD_LOAD_GLOBAL(thread_offset, g_thread_current_id);
    __asm__("nop\nsll $8,$8,10" : "=r"(thread_offset) : "0"(thread_offset));
    THREAD_LOAD_GLOBAL(threads, g_threads);
    __asm__("nop\nadd %0,%1,%2" : "=r"(thread) : "0"(thread_offset), "r"(threads));
    return thread->function_parameter_1;
}
#endif

#ifdef FFT_BATTLE
/* The original entry uses $at address loads and fixed $t0/$t1 scratch registers. */
__asm__(".set noat");

/* Return parameter 2 of the current thread's record. */
s32 thread_get_current_parameter_2_variant(void) {
    register s32 thread_offset __asm__("$8");
    register thread_t* threads __asm__("$9");
    register thread_t* thread __asm__("$8");

    THREAD_LOAD_GLOBAL(thread_offset, g_thread_current_id);
    __asm__("nop\nsll $8,$8,10" : "=r"(thread_offset) : "0"(thread_offset));
    THREAD_LOAD_GLOBAL(threads, g_threads);
    __asm__("nop\naddu %0,%1,%2" : "=r"(thread) : "0"(thread_offset), "r"(threads));
    return thread->function_parameter_2;
}
#endif

#ifdef FFT_WORLD
/* The original entry uses $at address loads and fixed $t0/$t1 scratch registers. */
__asm__(".set noat");

/* Return parameter 2 of the current thread's record. */
s32 thread_get_current_parameter_2_variant(void) {
    register s32 thread_offset __asm__("$8");
    register thread_t* threads __asm__("$9");
    register thread_t* thread __asm__("$8");

    THREAD_LOAD_GLOBAL(thread_offset, g_thread_current_id);
    __asm__("nop\nsll $8,$8,10" : "=r"(thread_offset) : "0"(thread_offset));
    THREAD_LOAD_GLOBAL(threads, g_threads);
    __asm__("nop\nadd %0,%1,%2" : "=r"(thread) : "0"(thread_offset), "r"(threads));
    return thread->function_parameter_2;
}
#endif

#ifdef FFT_BATTLE
/* The original entry uses $at address loads and fixed $t0/$t1 scratch registers. */
__asm__(".set noat");

/* Return parameter 3 of the current thread's record. */
s32 thread_get_current_parameter_3_variant(void) {
    register s32 thread_offset __asm__("$8");
    register thread_t* threads __asm__("$9");
    register thread_t* thread __asm__("$8");

    THREAD_LOAD_GLOBAL(thread_offset, g_thread_current_id);
    __asm__("nop\nsll $8,$8,10" : "=r"(thread_offset) : "0"(thread_offset));
    THREAD_LOAD_GLOBAL(threads, g_threads);
    __asm__("nop\naddu %0,%1,%2" : "=r"(thread) : "0"(thread_offset), "r"(threads));
    return thread->function_parameter_3;
}
#endif

#ifdef FFT_WORLD
/* The original entry uses $at address loads and fixed $t0/$t1 scratch registers. */
__asm__(".set noat");

/* Return parameter 3 of the current thread's record. */
s32 thread_get_current_parameter_3_variant(void) {
    register s32 thread_offset __asm__("$8");
    register thread_t* threads __asm__("$9");
    register thread_t* thread __asm__("$8");

    THREAD_LOAD_GLOBAL(thread_offset, g_thread_current_id);
    __asm__("nop\nsll $8,$8,10" : "=r"(thread_offset) : "0"(thread_offset));
    THREAD_LOAD_GLOBAL(threads, g_threads);
    __asm__("nop\nadd %0,%1,%2" : "=r"(thread) : "0"(thread_offset), "r"(threads));
    return thread->function_parameter_3;
}
#endif

#ifdef FFT_BATTLE
/* The original entry uses $at address loads and fixed $t0/$t1 scratch registers. */
__asm__(".set noat");

/* Read the preceding slot; slot 0 has no wraparound. */
s32 thread_is_previous_running_variant(void) {
    register s32 thread_offset __asm__("$8");
    register thread_t* threads __asm__("$9");
    register thread_t* thread __asm__("$8");

    THREAD_LOAD_GLOBAL(thread_offset, g_thread_current_id);
    __asm__("nop\naddi $8,$8,-1\nsll $8,$8,10" : "=r"(thread_offset) : "0"(thread_offset));
    THREAD_LOAD_GLOBAL(threads, g_threads);
    __asm__("nop\naddu %0,%1,%2" : "=r"(thread) : "0"(thread_offset), "r"(threads));
    return thread->is_running;
}
#endif

#ifdef FFT_WORLD
/* The original entry uses $at address loads and fixed $t0/$t1 scratch registers. */
__asm__(".set noat");

/* Read the preceding slot; slot 0 has no wraparound. */
s32 thread_is_previous_running_variant(void) {
    register s32 thread_offset __asm__("$8");
    register thread_t* threads __asm__("$9");
    register thread_t* thread __asm__("$8");

    THREAD_LOAD_GLOBAL(thread_offset, g_thread_current_id);
    __asm__("nop\naddi $8,$8,-1\nsll $8,$8,10" : "=r"(thread_offset) : "0"(thread_offset));
    THREAD_LOAD_GLOBAL(threads, g_threads);
    __asm__("nop\nadd %0,%1,%2" : "=r"(thread) : "0"(thread_offset), "r"(threads));
    return thread->is_running;
}
#endif

#ifdef FFT_BATTLE
/* The original entry uses $at address loads and fixed $t0/$t1 scratch registers. */
__asm__(".set noat");

/* Read the slot's running flag using the assembled entry's register sequence. */
s32 thread_is_running_asm_variant(s32 thread_id) {
    register s32 thread_offset __asm__("$8");
    register thread_t* threads __asm__("$9");
    register thread_t* thread __asm__("$8");

    thread_offset = thread_id << 10;
    THREAD_LOAD_GLOBAL(threads, g_threads);
    __asm__("nop\naddu %0,%1,%2" : "=r"(thread) : "0"(thread_offset), "r"(threads));
    return thread->is_running;
}
#endif

#ifdef FFT_WORLD
/* The original entry uses $at address loads and fixed $t0/$t1 scratch registers. */
__asm__(".set noat");

/* Read the slot's running flag using the assembled entry's register sequence. */
s32 thread_is_running_asm_variant(s32 thread_id) {
    register s32 thread_offset __asm__("$8");
    register thread_t* threads __asm__("$9");
    register thread_t* thread __asm__("$8");

    thread_offset = thread_id << 10;
    THREAD_LOAD_GLOBAL(threads, g_threads);
    __asm__("nop\nadd %0,%1,%2" : "=r"(thread) : "0"(thread_offset), "r"(threads));
    return thread->is_running;
}
#endif
