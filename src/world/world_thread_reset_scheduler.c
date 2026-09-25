#include "fft/world.h"

void world_thread_reset_scheduler(void) {
    s32* word;
    s32 i;

    /* Clear the whole 16 x 0x400 thread array, last word first. */
    word = (s32*)((u8*)g_world_threads + NATIVE_THREAD_LAST_WORD_OFFSET);
    i = (NATIVE_THREAD_ARRAY_BYTES / sizeof(s32)) - 1;
    do {
        *word = 0;
        i--;
        word--;
    } while (i >= 0);
    g_world_thread_current_id = 0;
    g_world_threads->is_running = 1;
}
