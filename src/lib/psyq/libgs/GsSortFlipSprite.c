#include "psx/libgs.h"
#include "psx/types.h"

/* GsSortSprite for unscaled, unrotated sprites: attribute
 * bits 22/23 flip the texture, bits 24-25 and 28-29 select the texture page
 * depth and blend mode, bit 30 semi-transparency and bit 6 unmodulated texture. */
void GsSortFlipSprite(GsSPRITE* sp, GsOT* otp, u16 pri) {
    POLY_FT4* p;
    u32 attr = sp->attribute;
    s32 x;
    s32 y;
    u8 u0;
    u8 u1;
    u8 v0;
    u8 v1;
    u32 mode; /* semi-transparency (attribute bit 30) in the code byte */
    u32 code; /* shading (attribute bit 6) plus the POLY_FT4 code 0x2c */

    if (!(attr & GsDOFF) && sp->w != 0 && sp->h != 0) {
        p = (POLY_FT4*)GsOUT_PACKET_P;
        x = sp->x + GsORGOFSX - sp->mx;
        y = sp->y + GsORGOFSY - sp->my;
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
        mode = (attr >> 5) & PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_SEMI_TRANSPARENT);
        code = ((attr << 18) & PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_RAW_TEXTURE))
            | PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_POLY_FT4);
        /* 32-bit GPU accesses preserve the retail stores; member stores narrow the writes. */
        *(u32*)&p->r0 = mode | code | (sp->b << 16) | (sp->g << 8) | sp->r;
        *(u32*)&p->x0 = (x & 0xffff) | (y << 16);
        *(u32*)&p->u0 = u0 | (v0 << 8) | (sp->cy << 22) | ((sp->cx << 12) & 0x3f0000);
        *(u32*)&p->x1 = ((x + sp->w) & 0xffff) | (y << 16);
        *(u32*)&p->u1
            = u1 | (v0 << 8) | ((sp->tpage & 0x1f) << 16) | ((attr >> 1) & 0x01800000) | ((attr >> 7) & 0x600000);
        *(u32*)&p->x2 = (x & 0xffff) | ((y + sp->h) << 16);
        *(u32*)&p->u2 = u0 | (v1 << 8);
        *(u32*)&p->x3 = ((x + sp->w) & 0xffff) | ((y + sp->h) << 16);
        *(u32*)&p->u3 = u1 | (v1 << 8);
        GsOUT_PACKET_P = psyq_gs_sort_sprite_bg((u32*)p, otp, pri, PSYQ_GPU_PACKET_WORDS(POLY_FT4));
    }
}
