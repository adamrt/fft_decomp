#include "fft/attack.h"
#include "psx/types.h"

void attack_gfx_init_frame_ft4_array(void) {
    s32 i = 0;
    s32 offset = sizeof(POLY_FT4);
    POLY_FT4* base = g_attack_gfx_frame_ft4;
    POLY_FT4* odd = base + 1;
    POLY_FT4* even = base;

    do {
        battle_gfx_init_default_poly_ft4(even);
        battle_gfx_init_default_poly_ft4(odd);
        /* offset is the byte offset of odd; [-1] is even. */
        ((POLY_FT4*)((u8*)g_attack_gfx_frame_ft4 + offset))[-1].tpage = GetTPage(0, 0, 0x140, 0);
        ((POLY_FT4*)((u8*)g_attack_gfx_frame_ft4 + offset))->tpage = GetTPage(0, 2, 0x3c0, 0x100);
        SetSemiTrans(odd, 1);
        offset += sizeof(POLY_FT4) * 2;
        odd += 2;
        i += 2;
        even += 2;
    } while (i < 10);
}
