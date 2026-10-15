#include "fft/thread.h"

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
