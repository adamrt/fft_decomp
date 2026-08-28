#include "fft/equip.h"
#include "psx/gpu.h"
#include "psx/types.h"

void equip_gfx_init_primitive_buffers(u8* buffers) {
    s32 i;

    for (i = 0; i < (s32)g_equip_gfx_line_f2_capacity; i++) {
        SetLineF2((void*)((i * 0x10) + *(s32*)(buffers + 0x24)));
    }
    for (i = 0; i < (s32)g_equip_gfx_poly_f4_capacity; i++) {
        SetPolyF4((POLY_F4*)((i * 0x18) + *(s32*)(buffers + 0xC)));
        SetShadeTex((void*)((i * 0x18) + *(s32*)(buffers + 0xC)), 0);
    }
    for (i = 0; i < (s32)g_equip_gfx_poly_ft4_capacity; i++) {
        SetPolyFT4((POLY_FT4*)((i * 0x28) + *(s32*)(buffers + 0x10)));
        SetShadeTex((void*)((i * 0x28) + *(s32*)(buffers + 0x10)), 0);
    }
    for (i = 0; i < (s32)g_equip_gfx_tile_capacity; i++) {
        SetTile((void*)((i * 0x10) + *(s32*)(buffers + 0x3C)));
    }
    for (i = 0; i < (s32)g_equip_gfx_draw_move_capacity; i++) {
        SetDrawMove((void*)((i * 0x18) + *(s32*)(buffers + 0x58)));
    }
}
