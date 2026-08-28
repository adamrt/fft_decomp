#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/gte_inline.h"
#include "psx/types.h"

/* LDV3 and STSXY3 as this routine issues them: all three operand copies
 * ($12-$14) precede the transfers, and each copy is its own statement so the
 * three addresses reuse one input register, as in the target. */
#define gte_ldv3_split(r0, r1, r2)                                                                                     \
    {                                                                                                                  \
        __asm__ volatile("addu $12, %0, $0" : : "r"(r0) : "$12");                                                      \
        __asm__ volatile("addu $13, %0, $0" : : "r"(r1) : "$13");                                                      \
        __asm__ volatile("addu $14, %0, $0" : : "r"(r2) : "$14");                                                      \
        __asm__ volatile("lwc2 $0, 0($12)");                                                                           \
        __asm__ volatile("lwc2 $1, 4($12)");                                                                           \
        __asm__ volatile("lwc2 $2, 0($13)");                                                                           \
        __asm__ volatile("lwc2 $3, 4($13)");                                                                           \
        __asm__ volatile("lwc2 $4, 0($14)");                                                                           \
        __asm__ volatile("lwc2 $5, 4($14)");                                                                           \
    }

#define gte_stsxy3_split(r0, r1, r2)                                                                                   \
    {                                                                                                                  \
        __asm__ volatile("addu $12, %0, $0" : : "r"(r0) : "$12");                                                      \
        __asm__ volatile("addu $13, %0, $0" : : "r"(r1) : "$13");                                                      \
        __asm__ volatile("addu $14, %0, $0" : : "r"(r2) : "$14");                                                      \
        __asm__ volatile("swc2 $12, 0($12)" : : : "memory");                                                           \
        __asm__ volatile("swc2 $13, 0($13)" : : : "memory");                                                           \
        __asm__ volatile("swc2 $14, 0($14)" : : : "memory");                                                           \
    }

/* One 7-byte sprite part: a signed offset, a size, a texture origin and flags
 * (0x02/0x04 mirror, 0x01 semi-transparent, 0x80 depth-sorted by camera pitch). */
typedef struct battle_gfx_sprite_part {
    s8 x;
    s8 y;
    u8 width;
    u8 height;
    u8 u;
    u8 v;
    u8 flags;
} battle_gfx_sprite_part_t;

typedef struct battle_gfx_sprite_display {
    u16 rg; /* 0x00; red and green of the primitive colour */
    u8 b;   /* 0x02 */
    u8 unknown_03;
    u16 tpage;                         /* 0x04 */
    u16 clut;                          /* 0x06 */
    u16 scale_x;                       /* 0x08 */
    u16 scale_y;                       /* 0x0a */
    u16 angle;                         /* 0x0c */
    battle_gfx_sprite_part_t parts[1]; /* 0x0e */
} battle_gfx_sprite_display_t;

/* POLY_FT4 with the colour's red/green pair addressed as one halfword. */
typedef struct battle_gfx_part_prim {
    u8 unknown_00[3];
    u8 length;
    u16 rg;
    u8 b;
    u8 code;
    s32 xy0;
    u8 u0;
    u8 v0;
    u16 clut;
    s32 xy1;
    u8 u1;
    u8 v1;
    u16 tpage;
    s32 xy2;
    u8 u2;
    u8 v2;
    u16 pad2;
    s32 xy3;
    u8 u3;
    u8 v3;
    u16 pad3;
} battle_gfx_part_prim_t;

/* Queue one POLY_FT4 per sprite part in [start, end) of a sprite display.
 *
 * The display's rotation, the caller's angle and scale, and position build the
 * GTE matrix; `mode` mirrors the parts (0x02/0x04), forces semi-transparency
 * (0x01) and selects the texture page's blend bits (0x60). Parts flagged 0x80
 * are depth-sorted by the camera pitch and zoom.
 *
 * The unused VECTOR reproduces the target's 16-byte locals area. The nested
 * test with its duplicated `addPrim(ot, prim)` arm is the original shape: jump
 * optimisation merges the arms, but the references counted before that merge
 * give `ot` the allocation priority that places it in $s5 ahead of `display`. */
void battle_gfx_construct_polygon_data_for_units(
    battle_gfx_sprite_display_t* display, s32 end, s32 start, s16* position, s16 angle, u16 mode, s16* scale, u32* ot) {
    VECTOR unused;
    battle_gfx_sprite_part_t* part;
    battle_gfx_part_prim_t* prim;
    s32 i;
    u16 flags;
    s16 index;
    s16 x;
    s16 y;
    s16 cosine;
    s16 sine;

    cosine = rcos(angle + display->angle);
    g_battle_gfx_numeric_display_matrix.m[0][0] = ((cosine * scale[0]) >> 12) * display->scale_x >> 12;
    g_battle_gfx_numeric_display_matrix.m[1][1] = ((cosine * scale[1]) >> 12) * display->scale_y >> 12;
    sine = rsin(angle + display->angle);
    g_battle_gfx_numeric_display_matrix.m[0][1] = ((-sine * scale[1]) >> 12) * display->scale_y >> 12;
    g_battle_gfx_numeric_display_matrix.m[1][0] = ((sine * scale[0]) >> 12) * display->scale_x >> 12;
    g_battle_gfx_numeric_display_matrix.t[0] = position[0];
    g_battle_gfx_numeric_display_matrix.t[1] = position[1];
    SetRotMatrix(&g_battle_gfx_numeric_display_matrix);
    SetTransMatrix(&g_battle_gfx_numeric_display_matrix);
    for (i = start; i < end; i++) {
        part = &display->parts[i];
        flags = part->flags;
        index = battle_gfx_increment_counter();
        flags ^= mode;
        prim = (battle_gfx_part_prim_t*)((POLY_FT4*)g_battle_unit_last_misc_init_byte + index);
        prim->length = 9;
        prim->code = 0x2c;
        if (mode & 2) {
            x = -part->x - part->width;
        } else {
            x = part->x;
        }
        if (mode & 4) {
            y = -part->y - part->height;
        } else {
            y = part->y;
        }
        if (flags & 2) {
            g_battle_gte_rtpt_vxy1.vx = g_battle_gte_rtps_vxy0.vx = x;
            g_battle_gte_rtpt_vxy0.vx = g_battle_gte_rtpt_vxy2.vx = x + part->width;
        } else {
            g_battle_gte_rtpt_vxy0.vx = g_battle_gte_rtpt_vxy2.vx = x;
            g_battle_gte_rtpt_vxy1.vx = g_battle_gte_rtps_vxy0.vx = x + part->width;
        }
        if (flags & 4) {
            g_battle_gte_rtpt_vxy2.vy = g_battle_gte_rtps_vxy0.vy = y;
            g_battle_gte_rtpt_vxy0.vy = g_battle_gte_rtpt_vxy1.vy = y + part->height;
        } else {
            g_battle_gte_rtpt_vxy0.vy = g_battle_gte_rtpt_vxy1.vy = y;
            g_battle_gte_rtpt_vxy2.vy = g_battle_gte_rtps_vxy0.vy = y + part->height;
        }
        gte_ldv3_split(&g_battle_gte_rtpt_vxy0, &g_battle_gte_rtpt_vxy1, &g_battle_gte_rtpt_vxy2);
        gte_rtpt();
        if ((flags | mode) & 1) {
            prim->code |= 2;
        } else {
            prim->code &= ~2;
        }
        prim->tpage = display->tpage | (mode & 0x60);
        prim->clut = display->clut;
        gte_stsxy3_split(&prim->xy0, &prim->xy1, &prim->xy2);
        gte_ldv0(&g_battle_gte_rtps_vxy0);
        gte_rtps();
        prim->rg = display->rg;
        prim->b = display->b;
        gte_stsxy(&prim->xy3);
        prim->u0 = prim->u2 = part->u;
        prim->u1 = prim->u3 = prim->u0 + part->width;
        prim->v0 = prim->v1 = part->v;
        prim->v2 = prim->v3 = prim->v0 + part->height;
        if (flags & 0x80) {
            if ((g_battle_camera_render_state.vx & 0xfff) < 0x400) {
                addPrim(ot - ((g_battle_camera_render_state.vx & 0xfff) - 0x12e) * g_battle_camera_zoom.vz / 368640 - 1,
                    prim);
            } else {
                addPrim(ot, prim);
            }
        } else {
            addPrim(ot, prim);
        }
    }
}
