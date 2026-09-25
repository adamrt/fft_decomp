#include "fft/battle.h"
#include "psx/types.h"

void battle_thread_wait_until_inactive(s32 thread_id) {
    s32 offset = thread_id * NATIVE_THREAD_STRIDE;
    do {
        battle_thread_yield();
    } while (*(s32*)(offset + (s32)g_battle_threads + 0x48) != 0);
}
