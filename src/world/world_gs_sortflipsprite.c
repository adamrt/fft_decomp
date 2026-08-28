#include "fft/world.h"
#include "psx/gs.h"
#include "psx/types.h"

extern u32 world_ps_sort_sprite_bg(u32* tag, GsOT* ot, s32 z, s32 len);

/* WORLD copy of libgs GsSortSprite for unscaled, unrotated sprites: attribute
 * bits 22/23 flip the texture, bits 24-25 and 28-29 select the texture page
 * depth and blend mode, bit 30 shading and bit 6 semi-transparency. */
void world_gs_sortflipsprite(const GsSPRITE* sp, void* otp, s32 pri) {
    world_poly_ft4_packet_t* p;
    s32 attr;
    s32 x;
    s32 y;
    u8 u0;
    u8 u1;
    u8 v0;
    u8 v1;
    u32 mode; /* semi-transparency (attribute bit 30) in the code byte */
    u32 code; /* shading (attribute bit 6) plus the POLY_FT4 code 0x2c */

    attr = sp->attribute;
    if (attr >= 0 && sp->w != 0 && sp->h != 0) {
        p = g_world_gs_out_packet_p;
        x = sp->x + g_world_gs_offset_x - sp->mx;
        y = sp->y + g_world_gs_offset_y - sp->my;
        if (attr & 0x800000) {
            u0 = sp->u + sp->w - 1;
            u1 = sp->u - 1;
        } else {
            u0 = sp->u;
            u1 = sp->u + sp->w;
        }
        if (attr & 0x400000) {
            v0 = sp->v + sp->h - 1;
            v1 = sp->v - 1;
        } else {
            v0 = sp->v;
            v1 = sp->v + sp->h;
        }
        /* Two temporaries: folded into one expression, GCC re-associates
         * the 0x2c000000 constant onto the first term. */
        mode = ((u32)attr >> 5) & 0x02000000;
        code = ((attr << 18) & 0x01000000) | 0x2c000000;
        p->mode = mode | code | (sp->b << 16) | (sp->g << 8) | sp->r;
        p->xy0 = (x & 0xffff) | (y << 16);
        p->uv0_clut = u0 | (v0 << 8) | (sp->cy << 22) | ((sp->cx << 12) & 0x3f0000);
        p->xy1 = ((x + sp->w) & 0xffff) | (y << 16);
        p->uv1_tpage = u1 | (v0 << 8) | ((sp->tpage & 0x1f) << 16) | (((u32)attr >> 1) & 0x01800000)
            | (((u32)attr >> 7) & 0x600000);
        p->xy2 = (x & 0xffff) | ((y + sp->h) << 16);
        p->uv2 = u0 | (v1 << 8);
        p->xy3 = ((x + sp->w) & 0xffff) | ((y + sp->h) << 16);
        p->uv3 = u1 | (v1 << 8);
        /* The target truncates pri to 16 bits at the call. */
        g_world_gs_out_packet_p = (void*)world_ps_sort_sprite_bg((u32*)p, otp, (u16)pri, 9);
    }
}
