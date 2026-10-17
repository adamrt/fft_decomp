#include "fft/suzuki.h"
#include "psx/types.h"

/* Seed the xorshift state that suzuki_smd_random steps (suzuki_sound_init seeds 0x12345678).
 * Nothing in the tree calls it. */
void suzuki_smd_seed_random(s32 value) {
    g_suzuki_smd_random_state = value;
}
