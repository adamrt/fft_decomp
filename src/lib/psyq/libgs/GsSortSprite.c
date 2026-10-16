#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* The target compares the adjacent neutral x/y scales with one 32-bit load. */
#define GS_SPRITE_SCALE_WORD(sp) (*(s32*)&sp->scalex)

/*
 * Attribute bit 27, or an unscaled, unrotated and unflipped sprite, takes the
 * fast path: a DR_MODE + SPRT packet. Otherwise the corners are rotated,
 * scaled and translated through the GTE into a POLY_FT4, with attribute bits
 * 23/22 flipping the texture horizontally/vertically. `rotate` is in 1/4096
 * degree units, so dividing by 360 gives the GTE's ONE-per-turn angle.
 *
 * The mode/shade/code temporaries keep GCC from re-associating the GPU code
 * constant onto the first operand, and the separate corner offsets keep
 * those values block-local.
 */
void GsSortSprite(GsSPRITE* sp, GsOT* ot, u16 pri) {
    MATRIX mat;
    SVECTOR rot;
    VECTOR vec;
    SVECTOR v0;
    SVECTOR v1;
    SVECTOR v2;
    SVECTOR v3;
    s32 sxy0;
    s32 sxy1;
    s32 sxy2;
    s32 sxy3;
    s32 depth;
    s32 flag;
    void* packet;
    SPRT* sprt;
    DR_TPAGE* draw_mode;
    POLY_FT4* poly;
    u32* words;
    u32 attr = sp->attribute;
    u32 code;
    u32 mode;
    u32 shade;
    u32 sprt_mode;
    u32 sprt_shade;
    u32 dr_mode;
    u32 sprt_code;
    s32 x;
    s32 ox;
    s32 oy;
    s32 y;
    u8 u_left;
    u8 u_right;
    u8 v_top;
    u8 v_bottom;

    if (!(attr & GsDOFF) && sp->w != 0 && sp->h != 0) {
        packet = GsOUT_PACKET_P;
        if (((attr >> 27) & 1)
            | (GS_SPRITE_SCALE_WORD(sp) == (ONE | (ONE << 16)) && sp->rotate == 0 && (attr & 0xc00000) == 0)) {
            draw_mode = packet;
            /* Share one DMA tag: the 32-bit mode command occupies this primitive view's tag slot. */
            sprt = (SPRT*)draw_mode->code;
            words = (u32*)sprt;
            x = sp->x + GsORGOFSX;
            y = sp->y + GsORGOFSY;
            dr_mode = (PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_MODE) | 0x200);
            draw_mode->code[0] = (sp->tpage & 0x1f) | (((attr >> 17) & 0x180) | dr_mode) | ((attr >> 23) & 0x60);
            sprt_mode = (attr >> 5) & PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_SEMI_TRANSPARENT);
            sprt_shade = (attr << 18) & PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_RAW_TEXTURE);
            sprt_code = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_SPRT);
            /* 32-bit indexing preserves scheduling; direct field-address stores change it. */
            words[offsetof(SPRT, r0) / sizeof(*words)]
                = sprt_mode | (sprt_shade | sprt_code) | (sp->b << 16) | (sp->g << 8) | sp->r;
            words[offsetof(SPRT, x0) / sizeof(*words)] = ((x - sp->mx) & 0xffff) | ((y - sp->my) << 16);
            words[offsetof(SPRT, u0) / sizeof(*words)]
                = sp->u | (sp->v << 8) | (sp->cy << 22) | ((sp->cx << 12) & 0x3f0000);
            words[offsetof(SPRT, w) / sizeof(*words)] = sp->w | (sp->h << 16);
            packet = psyq_gs_sort_sprite_bg(packet, ot, pri, PSYQ_GPU_PACKET_WORDS(SPRT) + 1);
        } else {
            if (sp->rotate == 0) {
                mat = GsIDMATRIX;
            } else {
                setVector(&rot, 0, 0, sp->rotate / 360);
                RotMatrix(&rot, &mat);
            }
            if (GS_SPRITE_SCALE_WORD(sp) != (ONE | (ONE << 16))) {
                setVector(&vec, sp->scalex, sp->scaley, 0);
                ScaleMatrix(&mat, &vec);
            }
            setVector(&vec, sp->x, sp->y, ReadGeomScreen());
            TransMatrix(&mat, &vec);
            SetRotMatrix(&mat);
            SetTransMatrix(&mat);
            ox = -sp->mx;
            oy = -sp->my;
            setVector(&v0, ox, oy, 0);
            setVector(&v1, ox + sp->w, oy, 0);
            setVector(&v2, ox, oy + sp->h, 0);
            setVector(&v3, ox + sp->w, oy + sp->h, 0);
            RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &depth, &flag);
            if (attr & 0x800000) {
                u_left = sp->u + sp->w - 1;
                u_right = sp->u;
            } else {
                u_left = sp->u;
                u_right = sp->u + sp->w - 1;
            }
            if (attr & 0x400000) {
                v_top = sp->v + sp->h - 1;
                v_bottom = sp->v;
            } else {
                v_top = sp->v;
                v_bottom = sp->v + sp->h - 1;
            }
            poly = packet;
            /* 32-bit GPU accesses preserve the shared tail store; member stores change scheduling. */
            words = (u32*)poly;
            mode = (attr >> 5) & PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_SEMI_TRANSPARENT);
            shade = (attr << 18) & PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_RAW_TEXTURE);
            code = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_POLY_FT4);
            words[offsetof(POLY_FT4, r0) / sizeof(*words)]
                = mode | (shade | code) | (sp->b << 16) | (sp->g << 8) | sp->r;
            words[offsetof(POLY_FT4, x0) / sizeof(*words)] = sxy0;
            words[offsetof(POLY_FT4, u0) / sizeof(*words)]
                = u_left | (v_top << 8) | (sp->cy << 22) | ((sp->cx << 12) & 0x3f0000);
            words[offsetof(POLY_FT4, x1) / sizeof(*words)] = sxy1;
            words[offsetof(POLY_FT4, u1) / sizeof(*words)] = u_right | (v_top << 8) | ((sp->tpage & 0x1f) << 16)
                | ((attr >> 1) & 0x01800000) | ((attr >> 7) & 0x600000);
            words[offsetof(POLY_FT4, x2) / sizeof(*words)] = sxy2;
            words[offsetof(POLY_FT4, u2) / sizeof(*words)] = u_left | (v_bottom << 8);
            words[offsetof(POLY_FT4, x3) / sizeof(*words)] = sxy3;
            words[offsetof(POLY_FT4, u3) / sizeof(*words)] = u_right | (v_bottom << 8);
            packet = psyq_gs_sort_sprite_bg(packet, ot, pri, PSYQ_GPU_PACKET_WORDS(POLY_FT4));
        }
        GsOUT_PACKET_P = packet;
    }
}
