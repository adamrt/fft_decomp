#include "fft/battle.h"
#include "psx/types.h"

u32 battle_gfx_read_compressed_nibble(void) {
    u32 nibble;
    u8 value;

    if (g_battle_gfx_compressed_nibble_phase == 0) {
        nibble = g_battle_gfx_compressed_data[g_battle_gfx_compressed_offset] >> 4;
    } else {
        value = g_battle_gfx_compressed_data[g_battle_gfx_compressed_offset];
        g_battle_gfx_compressed_offset++;
        nibble = value & 0xf;
    }
    g_battle_gfx_compressed_nibble_phase ^= 1;
    return nibble;
}
