#include "fft/battle_gfx.h"
#include "psx/types.h"

s16 battle_gfx_increment_counter(void) {
    g_battle_gfx_counter = (g_battle_gfx_counter + 1) % 0x100;
    return g_battle_gfx_counter;
}
