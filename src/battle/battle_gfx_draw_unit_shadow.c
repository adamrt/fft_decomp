#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Queue a unit's ground shadow quad.
 *
 * The four shadow corners at 0x29c are projected into a semi-transparent
 * POLY_FT4. On a tile with depth where screen_z is greater than
 * -(height + depth) * 12, the quad uses tpage 0x3f and UVs that rotate with
 * the counter at 0x29a; otherwise it uses tpage 0x5f and fixed UVs.
 * When the unit is above the ground height at its effect-adjusted position,
 * the quad is sorted by that ground point's depth; otherwise it goes into
 * the caller's ordering-table entry.
 *
 * `unused` reproduces the target's 0x58-byte frame. */
void battle_gfx_draw_unit_shadow(battle_unit_misc_data_t* unit, u32* ot) {
    VECTOR transformed;
    SVECTOR position;
    SVECTOR offset;
    SVECTOR unused;
    long flag;
    POLY_FT4* prim;
    map_tile_t* tile;
    SVECTOR* corners;
    u32* packet;
    s32 depth;
    u32 frame;

    if ((unit->shadow_dirty |= 1) != 0) {
        battle_gfx_calculate_sprite_shadow_from_tile_slope(unit);
        unit->shadow_dirty ^= 1;
    }
    SetRotMatrix(&g_battle_camera_matrix);
    SetTransMatrix(&g_battle_camera_matrix);
    prim = (POLY_FT4*)g_battle_unit_last_misc_init_byte + battle_gfx_increment_counter();
    corners = unit->shadow_quad;
    ((P_TAG*)prim)->len = 9;
    prim->code = 0x2c;
    RotTrans(&corners[0], &transformed, &flag);
    prim->x0 = transformed.vx;
    prim->y0 = transformed.vy;
    RotTrans(&corners[1], &transformed, &flag);
    prim->x1 = transformed.vx;
    prim->y1 = transformed.vy;
    RotTrans(&corners[2], &transformed, &flag);
    prim->x2 = transformed.vx;
    prim->y2 = transformed.vy;
    RotTrans(&corners[3], &transformed, &flag);
    prim->x3 = transformed.vx;
    prim->y3 = transformed.vy;
    prim->code |= 2;
    tile = battle_map_get_tile_data_pointer((s16)(unit->screen.vx / 28), (s16)(unit->screen.vz / 28), unit->map_z);
    if ((tile->depth_half_height & 0xe0) && -(tile->height + (tile->depth_half_height >> 5)) * 12 < unit->screen.vy) {
        unit->shadow_counter += g_animation_speed;
        frame = unit->shadow_counter;
        prim->tpage = 0x3f;
        prim->clut = unit->vram_palette_id + 0x40;
        prim->r0 = prim->g0 = prim->b0 = 0x80;
        switch (frame & 0x30) {
        case 0:
            prim->u0 = prim->u2 = 0xa8;
            prim->u1 = prim->u3 = 0xbb;
            prim->v0 = prim->v1 = 0x40;
            prim->v2 = prim->v3 = 0x53;
            break;
        case 0x10:
            prim->u2 = prim->u3 = 0xa8;
            prim->u0 = prim->u1 = 0xbb;
            prim->v0 = prim->v2 = 0x40;
            prim->v1 = prim->v3 = 0x53;
            break;
        case 0x20:
            prim->u1 = prim->u3 = 0xa8;
            prim->u0 = prim->u2 = 0xbb;
            prim->v2 = prim->v3 = 0x40;
            prim->v0 = prim->v1 = 0x53;
            break;
        case 0x30:
            prim->u0 = prim->u1 = 0xa8;
            prim->u2 = prim->u3 = 0xbb;
            prim->v1 = prim->v3 = 0x40;
            prim->v0 = prim->v2 = 0x53;
            break;
        }
    } else {
        prim->tpage = 0x5f;
        prim->clut = unit->vram_palette_id + 0x40;
        prim->r0 = prim->g0 = prim->b0 = 0x80;
        prim->u0 = prim->u2 = 0x90;
        prim->u1 = prim->u3 = 0xa3;
        prim->v0 = prim->v1 = 0x40;
        prim->v2 = prim->v3 = 0x53;
    }
    main_util_set_svector(&position, unit->screen.vx + unit->effect_vector_2.vx + unit->effect_vector.vx, 0,
        unit->screen.vz + unit->effect_vector_2.vz + unit->effect_vector.vz);
    position.vy = battle_map_calculate_slope_height(&position.vx, unit->map_z);
    if (unit->screen.vy >= position.vy) {
        packet = ot;
    } else {
        tile = battle_map_get_tile_data_pointer((s16)(position.vx / 28), (s16)(position.vz / 28), unit->map_z);
        position.vy -= (tile->depth_half_height >> 5) * 12;
        if (unit->centre_tile_offset != 0) {
            if (tile->surface.value & 0x40) {
                battle_camera_calculate_relative_offset_5((u16*)&position, &offset.vx);
            } else {
                battle_camera_calculate_relative_offset_1((u16*)&position, &offset.vx);
            }
        } else if (tile->surface.value & 0x40) {
            battle_camera_convert_screen_coords_modify_by_5(&position.vx, &offset.vx);
        } else {
            battle_camera_convert_screen_coords_modify_by_1(&position.vx, &offset.vx);
        }
        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_camera_matrix);
        RotTrans(&offset, &transformed, &flag);
        depth = transformed.vz / 4;
        packet = main_gfx_get_otag() + depth;
    }
    AddPrim(packet, prim);
}
