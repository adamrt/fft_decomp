#include "fft/battle.h"
#include "psx/types.h"

void battle_gfx_copy_other_spr_palette_data_to_ram(u8* src) {
    s32 i = 0;
    s32 one = 1; /* held in a local: a literal changes the setup schedule */
    u8* dst = &g_battle_gfx_chicken_palettes[0][0];
    do {
        u8 color_byte;
        if ((i & 1) && i % 32 != one) {
            color_byte = *src++ | 0x80;
        } else {
            color_byte = *src++;
        }
        *dst++ = color_byte;
        i++;
    } while (i < 0x2A0);
}
