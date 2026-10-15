#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Copy the first 21 OTHER.SPR palettes, setting STP on all but color zero. */
void battle_gfx_copy_other_spr_palette_data_to_ram(const battle_gfx_other_sprite_archive_t* archive) {
    s32 i = 0;
    s32 one = 1; /* held in a local: a literal changes the setup schedule */
    u8* dst = (u8*)&g_battle_gfx_other_palettes;
    /* Byte transfers preserve the palette loop and STP store sequence. */
    const u8* src = (const u8*)archive->palettes;
    do {
        u8 color_byte;
        if ((i & 1) && i % 32 != one) {
            color_byte = *src++ | 0x80;
        } else {
            color_byte = *src++;
        }
        *dst++ = color_byte;
        i++;
    } while (i < (s32)sizeof(g_battle_gfx_other_palettes));
}
