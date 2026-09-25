#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/types.h"

/*
 * Queue the four wait-direction arrows around a unit's feet, highlighting the
 * one that matches its facing quadrant.  Called from
 * battle_gfx_draw_wait_direction_unit with the unit's base position and the
 * ordering-table slot for its depth.
 *
 * The two initialized locals occupy 32 bytes of .rodata at 0x800677d8; the
 * arrow offsets are the four unit steps of length 14 around the tile.
 *
 * Each arm repeats the four V stores because the target hoists 0x9a and 0xa8
 * into saved registers, which only happens with more than one use apiece;
 * cross-jumping merges the repeats back into one tail.
 */
void battle_gfx_draw_wait_direction_arrows(SVECTOR* base, u32* ot) {
    SVECTOR position;
    VECTOR transformed;
    s32 dx[4] = { 0, -14, 0, 14 };
    s32 dz[4] = { -14, 0, 14, 0 };
    long flag;
    battle_unit_misc_data_t* unit;
    POLY_FT4* prim;
    /* Signed halfword: the target's division emits the `bgez`/`addiu 0x3ff`
     * correction, so the quadrant bits are not held in an unsigned type. */
    s16 facing_bits;
    s32 i;

    unit = battle_unit_get_source_misc_data();
    g_battle_gfx_wait_arrow_blink_timer += g_animation_speed;
    for (i = 0; i < 4; i++) {
        prim = (POLY_FT4*)g_battle_unit_last_misc_init_byte + battle_gfx_increment_counter();
        ((P_TAG*)prim)->len = 9;
        prim->code = 0x2e;
        prim->tpage = 0x3f;
        main_util_set_svector(&position, base->vx + dx[i], base->vy - 48, base->vz + dz[i]);
        RotTrans(&position, &transformed, &flag);
        prim->x0 = prim->x2 = transformed.vx - 7;
        prim->x1 = prim->x3 = transformed.vx + 7;
        prim->y0 = prim->y1 = transformed.vy - 7;
        prim->y2 = prim->y3 = transformed.vy + 7;
        prim->clut = GetClut(0x30, 0x1e2);
        prim->r0 = prim->g0 = prim->b0 = 0x80;
        facing_bits = unit->facing & 0xc00;
        if (i == facing_bits / 0x400) {
            if (g_battle_gfx_wait_arrow_blink_timer & 0x20) {
                prim->u0 = prim->u2 = 0xbc;
                prim->u1 = prim->u3 = 0xca;
                prim->v1 = 0x9a;
                prim->v0 = 0x9a;
                prim->v3 = 0xa8;
                prim->v2 = 0xa8;
            } else {
                prim->u0 = prim->u2 = 0xd6;
                prim->u1 = prim->u3 = 0xe4;
                prim->v1 = 0x9a;
                prim->v0 = 0x9a;
                prim->v3 = 0xa8;
                prim->v2 = 0xa8;
            }
        } else {
            prim->u0 = prim->u2 = 0xca;
            prim->u1 = prim->u3 = 0xd8;
            prim->v1 = 0x9a;
            prim->v0 = 0x9a;
            prim->v3 = 0xa8;
            prim->v2 = 0xa8;
        }
        AddPrim(ot, prim);
    }
}
