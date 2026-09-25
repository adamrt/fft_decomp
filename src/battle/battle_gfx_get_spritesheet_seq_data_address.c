#include "fft/battle.h"

u8* battle_gfx_get_spritesheet_seq_data_address(u32 index) {
    s32 value;

    value = g_battle_gfx_spritesheet_data[index & 0xffff].seq_id;
    if (value < 5) {
        return g_battle_gfx_spritesheet_alt_data + value * 0x408;
    }
    return g_battle_gfx_spritesheet_alt_fallback;
}
