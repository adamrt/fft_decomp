#include "fft/main_sound.h"
#include "psx/types.h"

/* xorshift step; returns the low 15 bits of the new state */
s32 main_smd_random(void) {
    s32 state;

    state = g_main_smd_random_state;
    state ^= state << 17;
    state ^= state >> 15;
    g_main_smd_random_state = state;
    return state & 0x7fff;
}
