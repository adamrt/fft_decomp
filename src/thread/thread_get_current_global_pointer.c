#include "fft/thread.h"

/* Capture $gp for a new native thread's saved context. */
void* thread_get_current_global_pointer(void) {
    void* global_pointer;

    __asm__("move %0, $gp" : "=r"(global_pointer));
    return global_pointer;
}
