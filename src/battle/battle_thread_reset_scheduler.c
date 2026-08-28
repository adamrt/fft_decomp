#include "fft/thread.h"
#include "psx/types.h"

/* Zeroes all 16 thread records (16 * 0x400 bytes), resets the current
 * thread id and marks thread 0's word 0x12 (offset 0x48). */
void battle_thread_reset_scheduler(void) {
    s32* word;
    s32 index;

    index = (NATIVE_THREAD_ARRAY_BYTES / sizeof(s32)) - 1;
    word = (s32*)((u8*)g_battle_threads + NATIVE_THREAD_LAST_WORD_OFFSET);
    do {
        *word = 0;
        index -= 1;
        word -= 1;
    } while (index >= 0);
    g_battle_current_thread_id = 0;
    g_battle_threads[0].is_running = 1;
}
