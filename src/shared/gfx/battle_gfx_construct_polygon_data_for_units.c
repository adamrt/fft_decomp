#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "psx/gte_inline.h"
#include "psx/gte_inline_split.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Queue one POLY_FT4 per sprite part in [start, end) of a sprite display.
 *
 * The display's rotation, the caller's angle and scale, and position build
 * the GTE matrix. `mode` supplies the whole-sprite flags and blend selector:
 * a flip bit mirrors each part's place about the sprite's origin, and its
 * picture unless the part's own flip bit is set too. Either one's
 * semi-transparent bit applies, and the blend bits of `mode` are added to
 * the display's texture page. A part flagged BATTLE_SPRITE_DISPLAY_NEARER is
 * queued ahead of `ot` by the camera pitch and zoom while the pitch is below
 * a quarter turn.
 *
 * The unused VECTOR reproduces the target's 16-byte locals area. The nested
 * test with its duplicated `addPrim(ot, prim)` arm is the original shape: jump
 * optimization merges the arms, but the references counted before that merge
 * give `ot` the allocation priority that places it in $s5 ahead of `display`. */
void battle_gfx_construct_polygon_data_for_units(battle_gfx_sprite_display_data_t* display, s32 end, s32 start,
    SVECTOR* position, s16 angle, s16 mode, SVECTOR* scale, u32* ot) {
    VECTOR unused;
    battle_gfx_sprite_part_display_data_t* part;
    POLY_FT4* prim;
    s32 i;
    s16 flags;
    s16 index;
    s16 x;
    s16 y;
    s16 cosine;
    s16 sine;

    cosine = rcos(angle + display->rotation);
    g_battle_gfx_numeric_display_matrix.m[0][0] = ((cosine * scale->vx) >> 12) * display->scale_x >> 12;
    g_battle_gfx_numeric_display_matrix.m[1][1] = ((cosine * scale->vy) >> 12) * display->scale_y >> 12;
    sine = rsin(angle + display->rotation);
    g_battle_gfx_numeric_display_matrix.m[0][1] = ((-sine * scale->vy) >> 12) * display->scale_y >> 12;
    g_battle_gfx_numeric_display_matrix.m[1][0] = ((sine * scale->vx) >> 12) * display->scale_x >> 12;
    g_battle_gfx_numeric_display_matrix.t[0] = position->vx;
    g_battle_gfx_numeric_display_matrix.t[1] = position->vy;
    SetRotMatrix(&g_battle_gfx_numeric_display_matrix);
    SetTransMatrix(&g_battle_gfx_numeric_display_matrix);
    for (i = start; i < end; i++) {
        part = &display->parts[i];
        flags = part->flags;
        index = battle_gfx_increment_counter();
        flags ^= mode;
        prim = g_battle_gfx_sprite_prim_pool + index;
        setlen(prim, 9);
        prim->code = 0x2c;
        if (mode & BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP) {
            x = -part->x_shift - part->width;
        } else {
            x = part->x_shift;
        }
        if (mode & BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP) {
            y = -part->y_shift - part->height;
        } else {
            y = part->y_shift;
        }
        if (flags & BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP) {
            g_battle_gte_rtpt_vxy1.vx = g_battle_gte_rtps_vxy0.vx = x;
            g_battle_gte_rtpt_vxy0.vx = g_battle_gte_rtpt_vxy2.vx = x + part->width;
        } else {
            g_battle_gte_rtpt_vxy0.vx = g_battle_gte_rtpt_vxy2.vx = x;
            g_battle_gte_rtpt_vxy1.vx = g_battle_gte_rtps_vxy0.vx = x + part->width;
        }
        if (flags & BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP) {
            g_battle_gte_rtpt_vxy2.vy = g_battle_gte_rtps_vxy0.vy = y;
            g_battle_gte_rtpt_vxy0.vy = g_battle_gte_rtpt_vxy1.vy = y + part->height;
        } else {
            g_battle_gte_rtpt_vxy0.vy = g_battle_gte_rtpt_vxy1.vy = y;
            g_battle_gte_rtpt_vxy2.vy = g_battle_gte_rtps_vxy0.vy = y + part->height;
        }
        gte_ldv3_split(&g_battle_gte_rtpt_vxy0, &g_battle_gte_rtpt_vxy1, &g_battle_gte_rtpt_vxy2);
        gte_rtpt();
        if ((flags | mode) & BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT) {
            prim->code |= 2;
        } else {
            prim->code &= ~2;
        }
        prim->tpage = display->tpage | (mode & BATTLE_SPRITE_DISPLAY_BLEND_MASK);
        prim->clut = display->clut;
        gte_stsxy3_split(&prim->x0, &prim->x1, &prim->x2);
        gte_ldv0(&g_battle_gte_rtps_vxy0);
        gte_rtps();
        /* The retail color transfer combines red and green in one 16-bit value. */
        *(u16*)&prim->r0 = *(u16*)&display->color.r;
        prim->b0 = display->color.b;
        gte_stsxy(&prim->x3);
        prim->u0 = prim->u2 = part->u;
        prim->u1 = prim->u3 = prim->u0 + part->width;
        prim->v0 = prim->v1 = part->v;
        prim->v2 = prim->v3 = prim->v0 + part->height;
        if (flags & BATTLE_SPRITE_DISPLAY_NEARER) {
            if ((g_camera_render_state.vx & 0xfff) < 0x400) {
                addPrim(ot - ((g_camera_render_state.vx & 0xfff) - 0x12e) * g_camera_zoom.vz / 368640 - 1, prim);
            } else {
                addPrim(ot, prim);
            }
        } else {
            addPrim(ot, prim);
        }
    }
}
