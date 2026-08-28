#include "fft/main_sound.h"
#include "psx/types.h"

void main_smd_seed_random(s32 value) {
    g_main_smd_random_state = value;
}
