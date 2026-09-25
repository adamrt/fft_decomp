#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/gs.h"
#include "psx/gte.h"
#include "psx/types.h"

/* DR_TPAGE + SPRT packet under one tag, as the packed words libgs writes. */
typedef struct {
    DR_TPAGE tpage; /* 0x00: tag and texture-page word */
    u32 mode;       /* 0x08: r | g << 8 | b << 16 | code << 24 */
    u32 xy;         /* 0x0c */
    u32 uv_clut;    /* 0x10 */
    u32 wh;         /* 0x14 */
} world_gs_sprite_packet_t;

/* The target tests scalex/scaley against the identity ONE/ONE (0x10001000) with a
 * single word load and compare. */
#define GS_SPRITE_SCALE_WORD(sp) (*(s32*)&(sp)->scalex)

extern s32 ReadGeomScreen(void);
extern s32 RotTransPers4(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, s32* sxy0, s32* sxy1, s32* sxy2, s32* sxy3, s32* p, s32* flag);

/*
 * WORLD copy of libgs GsSortSprite.
 *
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
void world_gs_sortsprite(GsSPRITE* sp, GsOT* ot, u16 pri) {
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
    world_gs_sprite_packet_t* sprt;
    world_poly_ft4_packet_t* poly;
    u32 attr;
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

    attr = sp->attribute;
    if ((s32)attr >= 0 && sp->w != 0 && sp->h != 0) {
        packet = g_world_gs_out_packet_p;
        if (((attr >> 27) & 1)
            | (GS_SPRITE_SCALE_WORD(sp) == 0x10001000 && sp->rotate == 0 && (attr & 0xc00000) == 0)) {
            sprt = packet;
            x = sp->x + g_world_gs_offset_x;
            y = sp->y + g_world_gs_offset_y;
            dr_mode = 0xe1000200;
            sprt->tpage.code[0] = (sp->tpage & 0x1f) | (((attr >> 17) & 0x180) | dr_mode) | ((attr >> 23) & 0x60);
            sprt_mode = (attr >> 5) & 0x02000000;
            sprt_shade = (attr << 18) & 0x01000000;
            sprt_code = 0x64000000;
            sprt->mode = sprt_mode | (sprt_shade | sprt_code) | (sp->b << 16) | (sp->g << 8) | sp->r;
            sprt->xy = ((x - sp->mx) & 0xffff) | ((y - sp->my) << 16);
            sprt->uv_clut = sp->u | (sp->v << 8) | (sp->cy << 22) | ((sp->cx << 12) & 0x3f0000);
            sprt->wh = sp->w | (sp->h << 16);
            packet = (void*)world_ps_sort_sprite_bg((u32*)packet, ot, pri, 5);
        } else {
            if (sp->rotate == 0) {
                mat = g_world_gs_id_matrix;
            } else {
                rot.vx = 0;
                rot.vy = 0;
                rot.vz = sp->rotate / 360;
                RotMatrix(&rot, &mat);
            }
            if (GS_SPRITE_SCALE_WORD(sp) != 0x10001000) {
                vec.vx = sp->scalex;
                vec.vy = sp->scaley;
                vec.vz = 0;
                ScaleMatrix(&mat, &vec);
            }
            vec.vx = sp->x;
            vec.vy = sp->y;
            vec.vz = ReadGeomScreen();
            TransMatrix(&mat, &vec);
            SetRotMatrix(&mat);
            SetTransMatrix(&mat);
            ox = -sp->mx;
            oy = -sp->my;
            v0.vx = ox;
            v0.vy = oy;
            v0.vz = 0;
            v1.vx = ox + sp->w;
            v1.vy = oy;
            v1.vz = 0;
            v2.vx = ox;
            v2.vy = oy + sp->h;
            v2.vz = 0;
            v3.vx = ox + sp->w;
            v3.vy = oy + sp->h;
            v3.vz = 0;
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
            mode = (attr >> 5) & 0x02000000;
            shade = (attr << 18) & 0x01000000;
            code = 0x2c000000;
            poly->mode = mode | (shade | code) | (sp->b << 16) | (sp->g << 8) | sp->r;
            poly->xy0 = sxy0;
            poly->uv0_clut = u_left | (v_top << 8) | (sp->cy << 22) | ((sp->cx << 12) & 0x3f0000);
            poly->xy1 = sxy1;
            poly->uv1_tpage = u_right | (v_top << 8) | ((sp->tpage & 0x1f) << 16) | ((attr >> 1) & 0x01800000)
                | ((attr >> 7) & 0x600000);
            poly->xy2 = sxy2;
            poly->uv2 = u_left | (v_bottom << 8);
            poly->xy3 = sxy3;
            poly->uv3 = u_right | (v_bottom << 8);
            packet = (void*)world_ps_sort_sprite_bg((u32*)packet, ot, pri, 9);
        }
        g_world_gs_out_packet_p = packet;
    }
}
