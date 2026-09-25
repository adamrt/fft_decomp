#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

/*
 * Queue the glow primitives for one cursor tile.
 *
 * The four vertices are projected with the battle camera. Shape 0 draws a textured
 * quad; shapes 1 and 2 split the tile into two textured triangles along either
 * diagonal. Each primitive gets a copy with the 0x1f texture page and the
 * 0x50 palette, and both are queued only when the first three projected
 * vertices face the camera. The pool stride is always sizeof(POLY_FT4).
 */

void battle_gfx_build_cursor_tile_glow(
    s32 palette, s32 shape, SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, u32* ot) {
    VECTOR screen;
    MATRIX unused_matrix; /* unreferenced; reserves the target frame's 0x20-byte gap at sp+0x20 */
    long flag;
    POLY_FT4* quad;
    POLY_FT4* quad_glow;
    POLY_FT3* tri_a;
    POLY_FT3* tri_b;
    POLY_FT3* tri_a_glow;
    POLY_FT3* tri_b_glow;

    SetRotMatrix(&g_battle_camera_matrix);
    SetTransMatrix(&g_battle_camera_matrix);
    switch (shape) {
    case 0:
        quad = (POLY_FT4*)g_battle_unit_last_misc_init_byte + battle_gfx_increment_counter();
        quad_glow = (POLY_FT4*)g_battle_unit_last_misc_init_byte + battle_gfx_increment_counter();
        ((P_TAG*)quad)->len = 9;
        quad->code = 0x2c;
        RotTrans(v0, &screen, &flag);
        quad->x0 = screen.vx;
        quad->y0 = screen.vy;
        RotTrans(v1, &screen, &flag);
        quad->x1 = screen.vx;
        quad->y1 = screen.vy;
        RotTrans(v2, &screen, &flag);
        quad->x2 = screen.vx;
        quad->y2 = screen.vy;
        RotTrans(v3, &screen, &flag);
        quad->x3 = screen.vx;
        quad->y3 = screen.vy;
        quad->code |= 2;
        quad->tpage = 0x3f;
        quad->clut = GetClut(palette * 16 + 16, 0x1e2);
        quad->r0 = quad->g0 = quad->b0 = 0x80;
        quad->u2 = 0;
        quad->u0 = 0;
        quad->u3 = 0xd;
        quad->u1 = 0xd;
        quad->v1 = 0xa0;
        quad->v0 = 0xa0;
        quad->v3 = 0xad;
        quad->v2 = 0xad;
        *quad_glow = *quad;
        quad_glow->tpage = 0x1f;
        quad_glow->clut = GetClut(0x50, 0x1e2);
        quad_glow->u2 = 0xc;
        quad_glow->u0 = 0xc;
        quad_glow->u3 = 0xc;
        quad_glow->u1 = 0xc;
        quad_glow->v1 = 0xa0;
        quad_glow->v0 = 0xa0;
        quad_glow->v3 = 0xa0;
        quad_glow->v2 = 0xa0;
        if (NormalClip((quad->y0 << 16) + quad->x0, (quad->y1 << 16) + quad->x1, (quad->y2 << 16) + quad->x2) >= 0) {
            AddPrim(ot, quad);
            AddPrim(ot, quad_glow);
        }
        return;
    case 1:
        tri_a = (POLY_FT3*)((POLY_FT4*)g_battle_unit_last_misc_init_byte + battle_gfx_increment_counter());
        tri_b = (POLY_FT3*)((POLY_FT4*)g_battle_unit_last_misc_init_byte + battle_gfx_increment_counter());
        tri_a_glow = (POLY_FT3*)((POLY_FT4*)g_battle_unit_last_misc_init_byte + battle_gfx_increment_counter());
        tri_b_glow = (POLY_FT3*)((POLY_FT4*)g_battle_unit_last_misc_init_byte + battle_gfx_increment_counter());
        ((P_TAG*)tri_a)->len = 7;
        tri_a->code = 0x24;
        ((P_TAG*)tri_b)->len = 7;
        tri_b->code = 0x24;
        RotTrans(v0, &screen, &flag);
        tri_a->x0 = screen.vx;
        tri_a->y0 = screen.vy;
        RotTrans(v1, &screen, &flag);
        tri_a->x1 = tri_b->x0 = screen.vx;
        tri_a->y1 = tri_b->y0 = screen.vy;
        RotTrans(v2, &screen, &flag);
        tri_a->x2 = tri_b->x2 = screen.vx;
        tri_a->y2 = tri_b->y2 = screen.vy;
        RotTrans(v3, &screen, &flag);
        tri_b->x1 = screen.vx;
        tri_b->y1 = screen.vy;
        tri_a->code |= 2;
        tri_b->code |= 2;
        tri_a->tpage = tri_b->tpage = 0x3f;
        tri_a->clut = tri_b->clut = GetClut(palette * 16 + 16, 0x1e2);
        tri_b->r0 = tri_b->g0 = tri_b->b0 = 0x80;
        tri_a->r0 = tri_a->g0 = tri_a->b0 = 0x80;
        tri_b->u2 = 0;
        tri_a->u2 = 0;
        tri_a->u0 = 0;
        tri_b->u1 = 0xd;
        tri_b->u0 = 0xd;
        tri_a->u1 = 0xd;
        tri_b->v0 = 0xa0;
        tri_a->v1 = 0xa0;
        tri_a->v0 = 0xa0;
        tri_b->v2 = 0xad;
        tri_b->v1 = 0xad;
        tri_a->v2 = 0xad;
        *tri_a_glow = *tri_a;
        *tri_b_glow = *tri_b;
        tri_a_glow->tpage = tri_b_glow->tpage = 0x1f;
        tri_a_glow->clut = tri_b_glow->clut = GetClut(0x50, 0x1e2);
        tri_b_glow->u2 = 0xc;
        tri_a_glow->u2 = 0xc;
        tri_a_glow->u0 = 0xc;
        tri_b_glow->u1 = 0xc;
        tri_b_glow->u0 = 0xc;
        tri_a_glow->u1 = 0xc;
        tri_b_glow->v0 = 0xa0;
        tri_a_glow->v1 = 0xa0;
        tri_a_glow->v0 = 0xa0;
        tri_b_glow->v2 = 0xa0;
        tri_b_glow->v1 = 0xa0;
        tri_a_glow->v2 = 0xa0;
        if (NormalClip((tri_a->y0 << 16) + tri_a->x0, (tri_a->y1 << 16) + tri_a->x1, (tri_a->y2 << 16) + tri_a->x2)
            >= 0) {
            AddPrim(ot, tri_a);
            AddPrim(ot, tri_a_glow);
        }
        if (NormalClip((tri_b->y0 << 16) + tri_b->x0, (tri_b->y1 << 16) + tri_b->x1, (tri_b->y2 << 16) + tri_b->x2)
            >= 0) {
            AddPrim(ot, tri_b);
            AddPrim(ot, tri_b_glow);
        }
        break;
    case 2:
        tri_a = (POLY_FT3*)((POLY_FT4*)g_battle_unit_last_misc_init_byte + battle_gfx_increment_counter());
        tri_b = (POLY_FT3*)((POLY_FT4*)g_battle_unit_last_misc_init_byte + battle_gfx_increment_counter());
        tri_a_glow = (POLY_FT3*)((POLY_FT4*)g_battle_unit_last_misc_init_byte + battle_gfx_increment_counter());
        tri_b_glow = (POLY_FT3*)((POLY_FT4*)g_battle_unit_last_misc_init_byte + battle_gfx_increment_counter());
        ((P_TAG*)tri_a)->len = 7;
        tri_a->code = 0x24;
        ((P_TAG*)tri_b)->len = 7;
        tri_b->code = 0x24;
        RotTrans(v0, &screen, &flag);
        tri_a->x0 = tri_b->x0 = screen.vx;
        tri_a->y0 = tri_b->y0 = screen.vy;
        RotTrans(v1, &screen, &flag);
        tri_a->x1 = screen.vx;
        tri_a->y1 = screen.vy;
        RotTrans(v2, &screen, &flag);
        tri_b->x2 = screen.vx;
        tri_b->y2 = screen.vy;
        RotTrans(v3, &screen, &flag);
        tri_a->x2 = tri_b->x1 = screen.vx;
        tri_a->y2 = tri_b->y1 = screen.vy;
        tri_a->code |= 2;
        tri_b->code |= 2;
        tri_a->tpage = tri_b->tpage = 0x3f;
        tri_a->clut = tri_b->clut = GetClut(palette * 16 + 16, 0x1e2);
        tri_b->r0 = tri_b->g0 = tri_b->b0 = 0x80;
        tri_a->r0 = tri_a->g0 = tri_a->b0 = 0x80;
        tri_b->u2 = 0;
        tri_b->u0 = 0;
        tri_a->u0 = 0;
        tri_b->u1 = 0xd;
        tri_a->u2 = 0xd;
        tri_a->u1 = 0xd;
        tri_b->v0 = 0xa0;
        tri_a->v1 = 0xa0;
        tri_a->v0 = 0xa0;
        tri_b->v2 = 0xad;
        tri_b->v1 = 0xad;
        tri_a->v2 = 0xad;
        *tri_a_glow = *tri_a;
        *tri_b_glow = *tri_b;
        tri_a_glow->tpage = tri_b_glow->tpage = 0x1f;
        tri_a_glow->clut = tri_b_glow->clut = GetClut(0x50, 0x1e2);
        tri_b_glow->u2 = 0xc;
        tri_a_glow->u2 = 0xc;
        tri_a_glow->u0 = 0xc;
        tri_b_glow->u1 = 0xc;
        tri_b_glow->u0 = 0xc;
        tri_a_glow->u1 = 0xc;
        tri_b_glow->v0 = 0xa0;
        tri_a_glow->v1 = 0xa0;
        tri_a_glow->v0 = 0xa0;
        tri_b_glow->v2 = 0xa0;
        tri_b_glow->v1 = 0xa0;
        tri_a_glow->v2 = 0xa0;
        if (NormalClip((tri_a->y0 << 16) + tri_a->x0, (tri_a->y1 << 16) + tri_a->x1, (tri_a->y2 << 16) + tri_a->x2)
            >= 0) {
            AddPrim(ot, tri_a);
            AddPrim(ot, tri_a_glow);
        }
        if (NormalClip((tri_b->y0 << 16) + tri_b->x0, (tri_b->y1 << 16) + tri_b->x1, (tri_b->y2 << 16) + tri_b->x2)
            >= 0) {
            AddPrim(ot, tri_b);
            AddPrim(ot, tri_b_glow);
        }
        break;
    }
}
