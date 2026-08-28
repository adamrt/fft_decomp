#include "psx/types.h"

void* world_thread_get_current_global_pointer(void) {
    register void* global_pointer __asm__("$28");
    register void* result __asm__("$2") = global_pointer;

    /* Emits nothing; without it GCC fills the jr delay slot with the move. */
    __asm__("" : : "r"(result));
    return result;
}
