#include "fft/main_runtime.h"
#include "fft/require.h"

void require_gfx_init_poly_gt4_array_8(s32 x, s32 y) {
    s32 count;
    s32 offset;
    s32 enabled;
    s16* texture_page;

    count = 0;
    enabled = 1;
    texture_page = &g_require_reward_bonus_palette_rect.y;
    offset = 0;
    do {
        POLY_GT4* poly = (POLY_GT4*)((u8*)g_require_gfx_poly_gt4_array_8 + offset);
        SetPolyGT4(poly);
        SetShadeTex(poly, 0);
        SetSemiTrans(poly, 1);
        poly->u0 = 0;
        poly->v0 = 0;
        poly->u1 = enabled;
        poly->v1 = 0;
        poly->u2 = 0;
        poly->v2 = enabled;
        poly->u3 = enabled;
        poly->v3 = enabled;
        poly->x0 = 0;
        poly->y0 = 0;
        poly->x1 = 0;
        poly->y1 = 0;
        poly->x2 = 0;
        poly->y2 = 0;
        poly->x3 = 0;
        poly->y3 = 0;
        if (GetGraphType() != enabled) {
            GetGraphType();
        }
        offset += sizeof(POLY_GT4);
        count++;
        poly->tpage = 6;
        poly->clut = ((((u16*)texture_page)[0] + y) << 6) | (((texture_page[-1] + x) >> 4) & 0x3f);
    } while (count < 8);
}
