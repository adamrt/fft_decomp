#include "fft/thread.h"

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
