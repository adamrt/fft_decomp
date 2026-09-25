#include "fft/battle.h"
#include "psx/types.h"

void battle_thread_wait_frames(s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        battle_thread_yield();
    }
}
