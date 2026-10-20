/* Draw the indicators around live units, including shadows, status bubbles, afterimages, held items and animated
 * action-result text. */

#include "fft/action.h"
#include "fft/animation.h"
#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/camera.h"
#include "fft/command.h"
#include "fft/cursor.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/move_step.h"
#include "fft/option.h"
#include "fft/result.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/state.h"
#include "fft/status.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "fft/vector.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Status flag shown by each status bubble before SOURCE_UNIT. DEAD and DEATH_SENTENCE hold 0 and are
 * tested separately by bubble_cycle_update; bubbles 8 and 11 hold 0 too,
 * so they never show. */
extern s32 g_status_bubble_status_masks[STATUS_BUBBLE_SOURCE_UNIT];

/* Frame counter and frame index of the selection cursor's bob animation. */
extern s32 g_cursor_selection_bob_timer;
extern s32 g_cursor_selection_bob_frame;

/* Advanced by g_system_vsync_interval once per call and sampled with bit 5 to blink
 * the highlighted arrow. */
extern u32 g_unit_facing_wait_arrow_blink_timer;

/* Texture coordinates for the status bubble graphics, indexed by status bubble
 * id (0x800949dc/0x800949f4) and by a counter digit (0x80094a0c/0x80094a24).
 * Each table holds 24 bytes; the pairs are read together as one u/v. */
static u8 g_bubble_u[24]
    = { 0, 16, 30, 44, 58, 72, 86, 100, 114, 128, 142, 0, 16, 30, 44, 58, 72, 86, 100, 86, 0, 0, 0, 0 };

static u8 g_bubble_v[24] = { 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 188, 188, 188, 188, 188, 188, 188,
    188, 200, 176, 0, 0, 0 };

static u8 g_bubble_digit_u[24]
    = { 142, 128, 114, 100, 86, 142, 128, 142, 128, 114, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static u8 g_bubble_digit_v[24]
    = { 200, 200, 200, 200, 200, 188, 188, 176, 176, 176, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* Post-action numeric and status displays. */
extern const u8 g_result_numeric_selectors[ACTION_DISPLAY_VALUE_COUNT];
extern u16 g_result_display_values[ACTION_DISPLAY_VALUE_COUNT];
extern const palette_t g_result_palette;

/* Eight per status_changes mask byte, indexed from the byte's high bit. */
extern u8 g_result_status_image_ids[STATUS_BYTE_COUNT][8];

/* Queue a unit's ground shadow quad.
 *
 * The four shadow_quad corners are projected into a semi-transparent POLY_FT4.
 * A unit below the water surface of its tile (world y greater than
 * -(height + depth) * 12, negative y being up) gets the additive water image of
 * the FRAME.BIN page, whose UVs turn a quarter every 16 counts of
 * shadow_counter (advanced by the frame interval); any other unit gets the
 * subtractive shadow image of the same page.
 *
 * A unit at or below the terrain under its event and SEQ adjusted position uses
 * the caller's ordering-table entry. A unit above it (jumping, floating or on
 * water) is sorted at the depth of the ground or water surface below it.
 *
 * The dirty test sets bit 0 of shadow_dirty before testing it, so the quad is
 * recalculated on every call.
 *
 * `unused` reproduces the target's 0x58-byte frame. */
void sprite_shadow_draw(unit_runtime_t* unit, u32* ot) {
    VECTOR transformed;
    SVECTOR position;
    SVECTOR offset;
    SVECTOR unused;
    s32 flag;
    POLY_FT4* prim;
    battle_map_tile_t* tile;
    SVECTOR* corners;
    u32* packet;
    s32 depth;
    u32 frame;

    if ((unit->shadow_dirty |= 1) != 0) {
        sprite_shadow_build_quad(unit);
        unit->shadow_dirty ^= 1;
    }
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    prim = g_sprite_prim_pool + sprite_primitive_take();
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
    tile = battle_map_tile_get_pointer((unit->world_coords.vx / BATTLE_MAP_TILE_SIZE),
        (unit->world_coords.vz / BATTLE_MAP_TILE_SIZE), unit->map_layer);
    if (tile->depth_half_height.depth
        && -(tile->height + tile->depth_half_height.depth) * BATTLE_MAP_HEIGHT_STEP < unit->world_coords.vy) {
        unit->shadow_counter += g_system_vsync_interval;
        frame = unit->shadow_counter;
        prim->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_ADD;
        prim->clut = unit->clut + 0x40; /* the unit's CLUT of BATTLE_MAP_CLUT_ROW_UNIT_SHADOWS */
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
        prim->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_SUBTRACT;
        prim->clut = unit->clut + 0x40; /* the unit's CLUT of BATTLE_MAP_CLUT_ROW_UNIT_SHADOWS */
        prim->r0 = prim->g0 = prim->b0 = 0x80;
        prim->u0 = prim->u2 = 0x90;
        prim->u1 = prim->u3 = 0xa3;
        prim->v0 = prim->v1 = 0x40;
        prim->v2 = prim->v3 = 0x53;
    }
    vector_short_set(&position, unit->world_coords.vx + unit->event_offset.vx + unit->seq_offset.vx, 0,
        unit->world_coords.vz + unit->event_offset.vz + unit->seq_offset.vz);
    position.vy = battle_map_slope_height_calculate(&position, unit->map_layer);
    if (unit->world_coords.vy >= position.vy) {
        packet = ot;
    } else {
        tile = battle_map_tile_get_pointer(
            (position.vx / BATTLE_MAP_TILE_SIZE), (position.vz / BATTLE_MAP_TILE_SIZE), unit->map_layer);
        position.vy -= tile->depth_half_height.depth * BATTLE_MAP_HEIGHT_STEP;
        if (unit->step_phase != MOVE_STEP_PHASE_NONE) {
            if (tile->surface.depth_bias) {
                camera_get_step_depth_point_biased(&position, &offset);
            } else {
                camera_get_step_depth_point_unbiased(&position, &offset);
            }
        } else if (tile->surface.depth_bias) {
            camera_get_tile_depth_point_biased(&position, &offset);
        } else {
            camera_get_tile_depth_point_unbiased(&position, &offset);
        }
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        RotTrans(&offset, &transformed, &flag);
        depth = transformed.vz / 4;
        packet = gfx_battle_otag_get() + depth;
    }
    AddPrim(packet, prim);
}

/*
 * Queue the glow primitives for one cursor tile.
 *
 * The four vertices are projected with the battle camera. Shape 0 (see
 * cursor_calculate_tile_polygon) draws a textured quad; shapes 1
 * and 2 split the tile into two textured triangles along the v0-v3 or v1-v2
 * diagonal. Each primitive draws the 13 x 13 glow image of the FRAME.BIN page
 * additively with CLUT palette + 1 of the sprite row, and a copy of it fills
 * the same area from a single texel with CLUT 5 at half blend. Both are queued
 * only when the first three projected vertices face the camera. Every
 * primitive takes a POLY_FT4-sized slot of the pool.
 */
void cursor_tile_glow_build(s32 palette, s32 shape, SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, u32* ot) {
    VECTOR screen;
    MATRIX unused_matrix; /* unreferenced; reserves the target frame's 0x20-byte gap at sp+0x20 */
    s32 flag;
    POLY_FT4* quad;
    POLY_FT4* quad_glow;
    POLY_FT3* tri_a;
    POLY_FT3* tri_b;
    POLY_FT3* tri_a_glow;
    POLY_FT3* tri_b_glow;

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    switch (shape) {
    case 0:
        quad = g_sprite_prim_pool + sprite_primitive_take();
        quad_glow = g_sprite_prim_pool + sprite_primitive_take();
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
        quad->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_ADD;
        quad->clut = GetClut(palette * 16 + 16, BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_SPRITES);
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
        quad_glow->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_AVERAGE;
        quad_glow->clut = GetClut(0x50, BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_SPRITES);
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
        tri_a = (POLY_FT3*)(g_sprite_prim_pool + sprite_primitive_take());
        tri_b = (POLY_FT3*)(g_sprite_prim_pool + sprite_primitive_take());
        tri_a_glow = (POLY_FT3*)(g_sprite_prim_pool + sprite_primitive_take());
        tri_b_glow = (POLY_FT3*)(g_sprite_prim_pool + sprite_primitive_take());
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
        tri_a->tpage = tri_b->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_ADD;
        tri_a->clut = tri_b->clut = GetClut(palette * 16 + 16, BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_SPRITES);
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
        tri_a_glow->tpage = tri_b_glow->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_AVERAGE;
        tri_a_glow->clut = tri_b_glow->clut = GetClut(0x50, BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_SPRITES);
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
        tri_a = (POLY_FT3*)(g_sprite_prim_pool + sprite_primitive_take());
        tri_b = (POLY_FT3*)(g_sprite_prim_pool + sprite_primitive_take());
        tri_a_glow = (POLY_FT3*)(g_sprite_prim_pool + sprite_primitive_take());
        tri_b_glow = (POLY_FT3*)(g_sprite_prim_pool + sprite_primitive_take());
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
        tri_a->tpage = tri_b->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_ADD;
        tri_a->clut = tri_b->clut = GetClut(palette * 16 + 16, BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_SPRITES);
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
        tri_a_glow->tpage = tri_b_glow->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_AVERAGE;
        tri_a_glow->clut = tri_b_glow->clut = GetClut(0x50, BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_SPRITES);
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

/*
 * Queue the map selection cursor over the cursor tile, whose surface point the
 * caller passes in `base` with the camera matrices loaded.
 *
 * The cursor floats 0x3c units above the tile, or 0x18 above the sprite height
 * of the unit the cursor selects there, scaled by the camera zoom. It bobs
 * down through an eight-frame pattern, advanced by the frame interval unless
 * the status screen is open. Its quad is the 16 x 24 cursor image scaled by
 * the camera zoom, sorted at the depth of the tile's surface point.
 *
 * The two initialized locals occupy 64 bytes of .rodata at 0x80067798: the bob
 * offsets and their durations in frames.
 */
void cursor_map_selection_draw(SVECTOR* base, u32* ot) {
    VECTOR transformed;
    SVECTOR screen;
    s32 bob[8] = { 0, 1, 2, 3, 5, 3, 2, 1 };
    s32 duration[8] = { 16, 8, 2, 2, 6, 4, 4, 10 };
    s32 flag;
    POLY_FT4* prim;
    unit_runtime_t* unit;
    s32 height;
    s32 half_height;

    prim = g_sprite_prim_pool + sprite_primitive_take();
    ((P_TAG*)prim)->len = 9;
    prim->code = 0x2c;
    if (g_menu_status_screen_selected != 1) {
        g_cursor_selection_bob_timer += g_system_vsync_interval;
        if (g_cursor_selection_bob_timer >= duration[g_cursor_selection_bob_frame]) {
            g_cursor_selection_bob_frame += 1;
            g_cursor_selection_bob_timer = 0;
            if (g_cursor_selection_bob_frame >= 8) {
                g_cursor_selection_bob_frame = 0;
            }
        }
    }
    unit = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
    if (unit != 0) {
        height = g_sprite_sheet_data[unit->spritesheet_id].graphic_height * g_camera_zoom.vy / ONE + 0x18;
    } else {
        height = 0x3c;
    }
    base->vy -= height - bob[g_cursor_selection_bob_frame];
    RotTrans(base, &transformed, &flag);

    prim->x0 = transformed.vx - g_camera_zoom.vx / 512;
    prim->y0 = transformed.vy - g_camera_zoom.vy * 12 / ONE;
    prim->x1 = transformed.vx + g_camera_zoom.vx / 512;
    prim->y1 = transformed.vy - g_camera_zoom.vy * 12 / ONE;
    prim->x2 = transformed.vx - g_camera_zoom.vx / 512;
    prim->y2 = transformed.vy + g_camera_zoom.vy * 12 / ONE;
    prim->x3 = transformed.vx + g_camera_zoom.vx / 512;
    half_height = g_camera_zoom.vy * 12 / ONE;
    {
        /* Read as a halfword, as the target does, after the half height. */
        u16 screen_y = transformed.vy;
        prim->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_SUBTRACT;
        prim->y3 = screen_y + half_height;
    }
    prim->code |= 2;
    prim->clut = GetClut(0x40, BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_SPRITES);
    prim->r0 = prim->g0 = prim->b0 = 0x80;
    prim->u2 = 0x40;
    prim->u0 = 0x40;
    prim->u3 = 0x50;
    prim->u1 = 0x50;
    prim->v1 = 0x80;
    prim->v0 = 0x80;
    prim->v3 = 0x98;
    prim->v2 = 0x98;
    camera_get_tile_depth_point_unbiased(base, &screen);
    RotTrans(&screen, &transformed, &flag);
    AddPrim(ot + (transformed.vz / 4), prim);
}

/*
 * Queue the four wait-direction arrows around a unit's feet. The arrow of the
 * source unit's facing quadrant blinks between two lit images every 32 timer
 * counts; the others show the unlit image. Called from
 * unit_facing_draw_wait_direction with the unit's base position and the
 * ordering-table slot for its depth.
 *
 * The two initialized locals occupy 32 bytes of .rodata at 0x800677d8; the
 * arrow offsets are the four unit steps of length 14 around the tile.
 *
 * Each arm repeats the four V stores because the target hoists 0x9a and 0xa8
 * into saved registers, which only happens with more than one use apiece;
 * cross-jumping merges the repeats back into one tail.
 */
void unit_facing_draw_wait_arrows(SVECTOR* base, u32* ot) {
    SVECTOR position;
    VECTOR transformed;
    s32 dx[4] = { 0, -14, 0, 14 };
    s32 dz[4] = { -14, 0, 14, 0 };
    s32 flag;
    unit_runtime_t* unit;
    POLY_FT4* prim;
    /* Signed 16-bit value: the target's division emits the `bgez`/`addiu 0x3ff`
     * correction, so the quadrant bits are not held in an unsigned type. */
    s16 facing_bits;
    s32 i;

    unit = unit_source_get();
    g_unit_facing_wait_arrow_blink_timer += g_system_vsync_interval;
    for (i = 0; i < 4; i++) {
        prim = g_sprite_prim_pool + sprite_primitive_take();
        ((P_TAG*)prim)->len = 9;
        prim->code = 0x2e;
        prim->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_ADD;
        vector_short_set(&position, base->vx + dx[i], base->vy - 48, base->vz + dz[i]);
        RotTrans(&position, &transformed, &flag);
        prim->x0 = prim->x2 = transformed.vx - 7;
        prim->x1 = prim->x3 = transformed.vx + 7;
        prim->y0 = prim->y1 = transformed.vy - 7;
        prim->y2 = prim->y3 = transformed.vy + 7;
        prim->clut = GetClut(0x30, BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_SPRITES);
        prim->r0 = prim->g0 = prim->b0 = 0x80;
        facing_bits = unit->facing & 0xc00;
        if (i == facing_bits / 0x400) {
            if (g_unit_facing_wait_arrow_blink_timer & 0x20) {
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

/* Defining the samples after their reader retains the original memory copy. */
static const s8 g_float_bob_offsets[24];

/* This updater needs a signed 16-bit result view. Changing the public return type
 * instead alters the arithmetic callers in unit_motion.c. */
typedef s16 (*unit_height_s16_fn_t)(unit_runtime_t* unit);

/* Raise a floating unit's height offset, or lower it back to the ground.
 *
 * A floating unit rises by the frame interval each update up to 12, then bobs through
 * g_float_bob_offsets (24 entries, two frames each) unless it is
 * a mount's rider. Otherwise the offset decays to 0, except during the
 * post-move event state. The unit's world and real y are then raised to the
 * recalculated ground height when they lie below it. Jumping units whose
 * height is controlled by their jump animation are left untouched. */
static void float_height_update(unit_runtime_t* unit) {
    s8 wave[24];
    s16 ground_y;
    u32 phase;

    memcpy(wave, g_float_bob_offsets, sizeof(wave));
    if (unit->status_flags_5_6 & (UNIT_STATUS_JUMP | UNIT_STATUS_JUMP_HEIGHT_ACTIVE)) {
        return;
    }
    if (unit->movement.floating) {
        if (unit->depth_height_offset < 12) {
            unit->depth_height_offset += g_system_vsync_interval;
        } else if (unit->mount_state != UNIT_MOUNT_STATE_RIDER) {
            phase = unit->float_bob_phase += g_system_vsync_interval;
            unit->float_bob_offset = wave[phase % 48 / 2];
        } else {
            unit->float_bob_offset = 0;
        }
    } else if (g_game_state != GAME_STATE_POST_MOVE_EVENTS) {
        if (unit->depth_height_offset != 0) {
            unit->depth_height_offset -= g_system_vsync_interval;
            if (unit->depth_height_offset < 0) {
                unit->depth_height_offset = 0;
            }
            ground_y = ((unit_height_s16_fn_t)unit_height_calculate_from_world_coords)(unit);
            unit->world_coords.vy = ground_y;
            unit->real.vy = ground_y * ONE;
        } else {
            unit->float_bob_offset = 0;
        }
    }
    ground_y = ((unit_height_s16_fn_t)unit_height_calculate_from_world_coords)(unit);
    if (unit->world_coords.vy > ground_y) {
        unit->world_coords.vy = ground_y;
        unit->real.vy = ground_y * ONE;
    }
}

/* Floating height offsets, one sample every two frames. */
static const s8 g_float_bob_offsets[24] = { 4, 4, 4, 4, 3, 3, 3, 2, 2, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 2, 2, 3, 3, 3 };

/* Advances one unit's status bubble by g_system_vsync_interval frames.
 *
 * The bubble switches texture rows every 16 timer ticks (CHARGING every 8).
 * Every 16 ticks the bubble id walks forward to the next of the
 * STATUS_BUBBLE_COUNT bubbles that applies, wrapping to DEAD, and the
 * bubble's x/y offsets are chosen from the unit's SHP type and animation. The
 * bubbles before SOURCE_UNIT follow statuses
 * (g_status_bubble_status_masks); SOURCE_UNIT belongs to the source
 * unit instead. The DEAD and DEATH_SENTENCE bubbles, which show counters, stay
 * while their status lasts. A unit with no bubble status that
 * is not the source unit loses its bubble.
 *
 * The block-scoped bubble_mask keeps GCC from rotating the loop's leading exit
 * tests to the bottom, and indexing g_status_bubble_status_masks inside the loop lets loop.c
 * strength-reduce it into the target's pointer walk. */
static void bubble_cycle_update(unit_runtime_t* unit) {
    s32 frame;
    s32 bubble;
    u32 status_flags;
    s32 animation;

    for (frame = 0; frame < g_system_vsync_interval; frame++) {
        bubble = unit->status_bubble_id;
        if (bubble == STATUS_BUBBLE_CHARGING) {
            unit->status_bubble_alternate_row = (unit->status_bubble_timer & 8) >> 3;
        } else {
            unit->status_bubble_alternate_row = (unit->status_bubble_timer & 0x10) >> 4;
        }

        if ((unit->status_bubble_timer & 0xF) == 0) {
            status_flags = unit->status_flags_1_4;
            if (((status_flags & UNIT_STATUS_BUBBLE_MASK) == 0) && (unit->runtime_id != g_unit_source_runtime_id)) {
                unit->status_bubble_active = 0;
                return;
            }

            for (;;) {
                s32 bubble_mask;

                if (bubble == STATUS_BUBBLE_DEAD) {
                    if (status_flags & UNIT_STATUS_DEAD) {
                        break;
                    }
                } else if (bubble == STATUS_BUBBLE_DEATH_SENTENCE) {
                    if (status_flags & UNIT_STATUS_DEATH_SENTENCE) {
                        break;
                    }
                }
                bubble++;
                if (bubble >= STATUS_BUBBLE_COUNT) {
                    bubble = STATUS_BUBBLE_DEAD;
                }
                if (bubble == unit->status_bubble_id) {
                    break;
                }
                if (bubble != STATUS_BUBBLE_SOURCE_UNIT) {
                    bubble_mask = g_status_bubble_status_masks[bubble];
                    if (status_flags & bubble_mask) {
                        break;
                    }
                } else if (unit->runtime_id == g_unit_source_runtime_id) {
                    break;
                }
            }
            unit->status_bubble_id = bubble;

            switch (g_sprite_sheet_data[unit->spritesheet_id].shp_id) {
            case SPRITE_SHP_ARUTE:
                unit->status_bubble_x = 0;
                unit->status_bubble_y = -0x46;
                break;
            case SPRITE_SHP_KANZEN:
                unit->status_bubble_x = 0;
                unit->status_bubble_y = -0x78;
                break;
            case SPRITE_SHP_OTHER:
                animation = unit->animation.sequence_id >> 1;
                if (animation == ANIMATION_DEAD || animation == ANIMATION_DEATH) {
                    unit->status_bubble_x = -5;
                    unit->status_bubble_y = -0x19;
                } else {
                    unit->status_bubble_x = 0;
                    unit->status_bubble_y = -0x19;
                }
                break;
            case SPRITE_SHP_TYPE1:
            case SPRITE_SHP_TYPE2:
                if (battle_map_tile_get_pointer(unit->map_x, unit->map_y, unit->map_layer)->depth_half_height.depth) {
                    switch (unit->animation.sequence_id >> 1) {
                    case ANIMATION_DEAD:
                    case ANIMATION_DEATH:
                        unit->status_bubble_x = -5;
                        unit->status_bubble_y = -0x23;
                        break;
                    case ANIMATION_SLEEP:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x1E;
                        break;
                    default:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x28;
                        break;
                    }
                } else {
                    switch (unit->animation.sequence_id >> 1) {
                    case ANIMATION_DEAD:
                    case ANIMATION_DEATH:
                        unit->status_bubble_x = -5;
                        unit->status_bubble_y = -0x19;
                        break;
                    case ANIMATION_SLEEP:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x1E;
                        break;
                    default:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x28;
                        break;
                    }
                }
                break;
            default:
                animation = unit->animation.sequence_id >> 1;
                if (animation == ANIMATION_DEAD || animation == ANIMATION_DEATH) {
                    unit->status_bubble_x = -5;
                    unit->status_bubble_y = -0x1E;
                } else {
                    unit->status_bubble_x = 0;
                    unit->status_bubble_y = -0x32;
                }
                break;
            }
        }
        unit->status_bubble_timer++;
    }
}

/* Unit scale for status bubbles and post-action text. */
static const SVECTOR g_identity_scale = { ONE, ONE, ONE, 0 };

/*
 * Queue the unit's status bubble into the ordering table.
 *
 * The bubble's texture coordinates are chosen from the status bubble id. The
 * DEAD bubble shows the RERAISE image on a unit with Reraise, else the digit
 * of its death counter, else (counter 0xff) its own image. The DEATH_SENTENCE
 * bubble shows the digit of that status's count. DEAD, CHARGING, RERAISE and
 * SOURCE_UNIT have a second texture row, selected by
 * status_bubble_alternate_row. The bubble is drawn at status_bubble_y (negative
 * is up) from the unit's feet and status_bubble_x across, one pixel higher
 * while bit 4 of the timer is set, and sorted at the depth of a point 12 units
 * below its base.
 */
void sprite_status_bubble_draw(unit_runtime_t* unit, const DVECTOR* unused_screen_coords) {
    SVECTOR position;
    SVECTOR zoom = g_identity_scale;
    VECTOR transformed;
    SVECTOR base;
    SVECTOR raised;
    s32 flag;
    sprite_display_t* display;
    s32 bubble_id;
    /* One local for both digit indices: the status-CT arm reuses the dying
     * counter's register, and two separate locals allocate differently. */
    s32 table_index;
    u8 row;
    u8 flag_row;
    u8 default_row;
    s32 depth;
    s16 y;
    u32* otag;

    /* The caller supplies its culling coordinates in a1, but this routine
     * independently projects the bubble and never reads them. */
    (void)unused_screen_coords;

    bubble_id = unit->status_bubble_id;
    display = unit->status_bubble_display;
    if (bubble_id == STATUS_BUBBLE_SOURCE_UNIT) {
        display->parts[0].u = 0x72;
        display->parts[0].v = 0xB0;
        if (unit->status_bubble_alternate_row != 0) {
            display->parts[0].v = 0xBC;
        }
    } else if (bubble_id == STATUS_BUBBLE_DEAD) {
        table_index = unit->stats->death_counter;
        if (unit->status_flags_1_4 & UNIT_STATUS_RERAISE) {
            display->parts[0].u = g_bubble_u[STATUS_BUBBLE_RERAISE];
            flag_row = g_bubble_v[STATUS_BUBBLE_RERAISE];
            display->parts[0].v = flag_row;
            if (unit->status_bubble_alternate_row != 0) {
                display->parts[0].v = flag_row + 12;
            }
        } else if (table_index != 0xFF) {
            display->parts[0].u = g_bubble_digit_u[table_index];
            display->parts[0].v = g_bubble_digit_v[table_index];
        } else {
            display->parts[0].u = g_bubble_u[STATUS_BUBBLE_DEAD];
            default_row = g_bubble_v[STATUS_BUBBLE_DEAD];
            display->parts[0].v = default_row;
            if (unit->status_bubble_alternate_row != 0) {
                display->parts[0].v = default_row + 12;
            }
        }
    } else if (bubble_id == STATUS_BUBBLE_DEATH_SENTENCE) {
        table_index = unit->stats->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)] & 3;
        display->parts[0].u = g_bubble_digit_u[table_index];
        display->parts[0].v = g_bubble_digit_v[table_index];
    } else {
        display->parts[0].u = g_bubble_u[bubble_id];
        row = g_bubble_v[bubble_id];
        display->parts[0].v = row;
        if (bubble_id == STATUS_BUBBLE_CHARGING || bubble_id == STATUS_BUBBLE_RERAISE) {
            if (unit->status_bubble_alternate_row != 0) {
                display->parts[0].v = row + 12;
            }
        }
    }

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    vector_short_set(&base, unit->world_coords.vx + unit->event_offset.vx,
        unit->world_coords.vy + unit->event_offset.vy + unit->status_bubble_y,
        unit->world_coords.vz + unit->event_offset.vz);
    RotTrans(&base, &transformed, &unit->gte_flag);
    position.vx = unit->status_bubble_x + (transformed.vx + unit->screen_offset.vx);
    y = transformed.vy + unit->screen_offset.vy;
    position.vy = y;
    if (unit->status_bubble_timer & 0x10) {
        position.vy = y - 1;
    }
    camera_get_tile_depth_point_unbiased(&base, &raised);
    raised.vy += 12;
    RotTrans(&raised, &transformed, &flag);
    display = unit->status_bubble_display;
    otag = gfx_battle_otag_get();
    depth = transformed.vz / 4;
    sprite_display_construct_polygon_data(display, display->part_count, 0, &position, 0, 0, &zoom, otag + depth);
}

/* Per-frame float and status-bubble update of one unit: adjusts its float height
 * except during the four float-jump step phases, then advances its status bubble
 * when one is active. */
void unit_float_status_bubble_update(unit_runtime_t* unit) {
    u8 phase = unit->step_phase;

    if (phase != MOVE_STEP_PHASE_FLOAT_JUMP_SOUTH && phase != MOVE_STEP_PHASE_FLOAT_JUMP_EAST
        && phase != MOVE_STEP_PHASE_FLOAT_JUMP_WEST && phase != MOVE_STEP_PHASE_FLOAT_JUMP_NORTH) {
        float_height_update(unit);
    }
    if (unit->status_bubble_active != 0) {
        bubble_cycle_update(unit);
    }
}

/* Start the unit's afterimage trail: enable position_copies and seed all six at its world
 * position. Jump launches and SEQ opcode 0xbf start it. */
void sprite_afterimage_init(unit_runtime_t* unit) {
    s32 i;

    unit->position_copies_active = 1;
    for (i = 0; i < 6; i++) {
        vector_short_set(
            &unit->position_copies[i], unit->world_coords.vx, unit->world_coords.vy, unit->world_coords.vz);
    }
}

/*
 * Draw the unit's current sprite frame once at each of its six position_copies:
 * the afterimage trail that sprite_afterimage_init starts.
 *
 * Each copy is projected with the battle camera and queued at its own
 * ordering-table depth using the current camera zoom, semi-transparent with the
 * blend bits cleared (half blend). The camera matrices are rebuilt for every
 * copy.
 */
void sprite_afterimage_draw(unit_runtime_t* unit) {
    SVECTOR position;
    SVECTOR zoom;
    VECTOR transformed;
    s32 flag;
    sprite_display_t* display;
    s32 depth;
    s32 i;
    s16 flags;

    for (i = 0; i < 6; i++) {
        camera_matrices_update(&g_camera_matrix, &g_camera_render_state, &g_camera_screen_offset, &g_camera_zoom);
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        RotTrans(&unit->position_copies[i], &transformed, &flag);
        depth = transformed.vz / 4;
        position.vx = transformed.vx;
        position.vy = transformed.vy;
        flags = ((unit->horizontal_flip_flag ^ unit->sprite_display_flags) & ~SPRITE_DISPLAY_BLEND_MASK)
            | SPRITE_DISPLAY_SEMI_TRANSPARENT;
        copyVector(&zoom, &g_camera_zoom);
        display = unit->animation.display;
        sprite_display_construct_polygon_data(
            display, display->part_count, 0, &position, 0, flags, &zoom, gfx_battle_otag_get() + depth);
    }
}

/* Advances the unit's position trail: each copy takes the next newer one and copy
 * 0 the unit's current position. */
void sprite_afterimage_shift_and_fill(unit_runtime_t* unit) {
    s32 index;

    for (index = 5; index > 0; index--) {
        unit->position_copies[index] = unit->position_copies[index - 1];
    }
    unit_draw_coords_copy(unit->position_copies, unit);
}

/* Make the unit's item display show an item's 16x16 icon, centered on the
 * display's origin. */
void sprite_display_store_item(unit_runtime_t* unit, u32 item_id) {
    SPRT data;

    gfx_set_item_icon_sprite(&data, item_id);
    unit->item_display->clut = data.clut;
    sprite_display_store_data(
        unit->item_display, 0, (16 - data.w) / 2 - 8, (16 - data.h) / 2 - 8, data.u0, data.v0, data.w, data.h, 0);
}

/* Queues the unit's item icon at its item display offset from the unit's screen
 * position, at the unit's ordering-table depth and scale ONE. The x offset is
 * negated unless the sprite is flipped horizontally. */
void sprite_item_display_draw(unit_runtime_t* unit) {
    SVECTOR position;
    SVECTOR zoom;
    u32* otag;

    if ((unit->sprite_display_flags & SPRITE_DISPLAY_HORIZONTAL_FLIP) != 0) {
        position.vx = unit->item_display_x_offset;
    } else {
        position.vx = -unit->item_display_x_offset;
    }
    position.vy = unit->item_display_y_offset;
    position.vx += unit->camera_relative_x;
    position.vy += unit->camera_relative_y;
    vector_short_set(&zoom, ONE, ONE, ONE);
    otag = gfx_battle_otag_get();
    otag += unit->otag_depth_index;
    sprite_display_construct_polygon_data(
        unit->item_display, unit->item_display->part_count, 0, &position, 0, 0, &zoom, otag);
}

/* Load the unit's next pending action result into its numeric display and clear its pending flag:
 * a stat change, then level down or up, stolen, broken, Quick, CT 0, missed, guarded, caught, then
 * the queued status images. Sets the digit sprites (values over 999 show 999) and the color.
 * Returns 0 when nothing is pending. */
s32 result_next_display_build(unit_runtime_t* unit) {
    unit_stats_t* stats;
    u32 scan_flags;
    s32 flags;
    action_result_display_t* display;
    sprite_display_t* display_0;
    sprite_display_t* display_1;
    sprite_display_t* display_2;
    const action_display_image_rect_t* image;
    s32 flag;
    s32 index;
    s32 digits[3];
    u8 red;
    u8 green;
    u8 blue;
    s16 y_shift;
    s32 image_id;
    u8 change;
    u8 status;

    flags = unit->action_display_flags;
    stats = unit->stats;
    display = &unit->numeric_display;

    if ((flags & ACTION_DISPLAY_NUMERIC_FLAGS_MASK) != 0) {
        g_result_display_values[RESULT_DISPLAY_HP_DAMAGE] = stats->action.hp_damage;
        g_result_display_values[RESULT_DISPLAY_HP_HEALING] = stats->action.hp_healing;
        g_result_display_values[RESULT_DISPLAY_MP_DAMAGE] = stats->action.mp_damage;
        g_result_display_values[RESULT_DISPLAY_MP_HEALING] = stats->action.mp_healing;
        /* A stat's gain and damage slots share one magnitude; the pending flag picks which shows. */
        change = stats->action.sp_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_result_display_values[RESULT_DISPLAY_SPEED_GAIN] = change;
        g_result_display_values[RESULT_DISPLAY_SPEED_DAMAGE] = change;
        change = stats->action.ct_change & 0x7f;
        g_result_display_values[RESULT_DISPLAY_CT_GAIN] = change;
        g_result_display_values[RESULT_DISPLAY_CT_DAMAGE] = change;
        change = stats->action.brave_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_result_display_values[RESULT_DISPLAY_BRAVE_GAIN] = change;
        g_result_display_values[RESULT_DISPLAY_BRAVE_DAMAGE] = change;
        change = stats->action.faith_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_result_display_values[RESULT_DISPLAY_FAITH_GAIN] = change;
        g_result_display_values[RESULT_DISPLAY_FAITH_DAMAGE] = change;
        flag = 1;
        change = stats->action.pa_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_result_display_values[RESULT_DISPLAY_PA_GAIN] = change;
        g_result_display_values[RESULT_DISPLAY_PA_DAMAGE] = change;
        index = 0;
        change = stats->action.ma_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_result_display_values[RESULT_DISPLAY_MA_GAIN] = change;
        g_result_display_values[RESULT_DISPLAY_MA_DAMAGE] = change;

        /* Show the lowest pending numeric result and clear its flag. */
        scan_flags = unit->action_display_flags;
        while (index < ACTION_DISPLAY_VALUE_COUNT) {
            if ((flag & scan_flags) != 0) {
                flags = index;
                display->value = g_result_display_values[flags];
                display->selector = g_result_numeric_selectors[flags];
                index = unit->action_display_flags;
                unit->action_display_flags = ~flag & index;
                break;
            }
            index++;
            flag <<= 1;
        }
    } else if ((flags & ACTION_DISPLAY_FLAG_LEVEL_DOWN) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = RESULT_DISPLAY_LEVEL_DOWN;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_LEVEL_DOWN;
    } else if ((flags & ACTION_DISPLAY_FLAG_LEVEL_UP) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = RESULT_DISPLAY_LEVEL_UP;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_LEVEL_UP;
    } else if ((flags & ACTION_DISPLAY_FLAG_STOLEN) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = RESULT_DISPLAY_STOLEN;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_STOLEN;
    } else if ((flags & ACTION_DISPLAY_FLAG_BROKEN) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = RESULT_DISPLAY_BROKEN;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_BROKEN;
    } else if ((flags & ACTION_DISPLAY_FLAG_QUICK) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = RESULT_DISPLAY_QUICK;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_QUICK;
    } else if ((flags & ACTION_DISPLAY_FLAG_CT_ZERO) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = RESULT_DISPLAY_CT_ZERO;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_CT_ZERO;
    } else if ((flags & ACTION_DISPLAY_FLAG_MISSED) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = RESULT_DISPLAY_MISSED;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_MISSED;
    } else if ((flags & ACTION_DISPLAY_FLAG_GUARDED) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = RESULT_DISPLAY_GUARDED;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_GUARDED;
    } else if ((flags & ACTION_DISPLAY_FLAG_CAUGHT) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = RESULT_DISPLAY_CAUGHT;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_CAUGHT;
    } else if (unit->status_display_image_count != 0) {
        unit->status_display_image_count--;
        status = unit->status_display_image_list[unit->status_display_image_count];
        if ((status & ACTION_STATUS_DISPLAY_REMOVAL) != 0) {
            display->selector = RESULT_DISPLAY_STATUS_REMOVED;
        } else {
            display->selector = RESULT_DISPLAY_STATUS_INFLICTED;
        }
        status &= ACTION_STATUS_DISPLAY_IMAGE_MASK;
    } else {
        return 0;
    }

    if (unit->stats->action.critical != 0) {
        display->selector |= RESULT_DISPLAY_MODIFIER_CRITICAL;
    } else {
        display->selector &= ~RESULT_DISPLAY_MODIFIER_CRITICAL;
    }

    display->active = 1;
    if (display->progress >= 22) {
        display->progress = 21;
    }

    /* Every message produced above selects a color case.  The target has no
     * default initialization for these three registers. */
    switch (display->selector & RESULT_DISPLAY_SELECTOR_MASK) {
    case RESULT_DISPLAY_HP_DAMAGE:
    case RESULT_DISPLAY_SPEED_DAMAGE:
    case RESULT_DISPLAY_CT_DAMAGE:
    case RESULT_DISPLAY_BRAVE_DAMAGE:
    case RESULT_DISPLAY_FAITH_DAMAGE:
    case RESULT_DISPLAY_PA_DAMAGE:
    case RESULT_DISPLAY_MA_DAMAGE:
    case RESULT_DISPLAY_STATUS_INFLICTED:
    case RESULT_DISPLAY_LEVEL_DOWN:
    case RESULT_DISPLAY_STOLEN:
    case RESULT_DISPLAY_BROKEN:
    case RESULT_DISPLAY_QUICK:
    case RESULT_DISPLAY_CT_ZERO:
        blue = 128;
        green = 128;
        red = 128;
        break;
    case RESULT_DISPLAY_MP_DAMAGE:
        red = 120;
        green = 60;
        blue = 220;
        break;
    case RESULT_DISPLAY_HP_HEALING:
    case RESULT_DISPLAY_SPEED_GAIN:
    case RESULT_DISPLAY_CT_GAIN:
    case RESULT_DISPLAY_BRAVE_GAIN:
    case RESULT_DISPLAY_FAITH_GAIN:
    case RESULT_DISPLAY_PA_GAIN:
    case RESULT_DISPLAY_MA_GAIN:
    case RESULT_DISPLAY_STATUS_REMOVED:
    case RESULT_DISPLAY_LEVEL_UP:
        red = 40;
        green = 140;
        blue = 220;
        break;
    case RESULT_DISPLAY_MP_HEALING:
        red = 78;
        green = 140;
        blue = 30;
        break;
    case RESULT_DISPLAY_MISSED:
    case RESULT_DISPLAY_GUARDED:
    case RESULT_DISPLAY_CAUGHT:
        red = 190;
        green = 40;
        blue = 10;
        break;
    }

    display->displays[0]->color.r = display->displays[1]->color.r = display->displays[2]->color.r = red;
    display->displays[0]->color.g = display->displays[1]->color.g = display->displays[2]->color.g = green;
    display->displays[0]->color.b = display->displays[1]->color.b = display->displays[2]->color.b = blue;

    if (display->value / 1000) {
        digits[2] = 9;
        digits[1] = 9;
        digits[0] = 9;
    } else {
        digits[0] = display->value % 10;
        digits[1] = (display->value / 10) % 10;
        digits[2] = display->value / 100;
    }

    display_0 = display->displays[0];
    display_1 = display->displays[1];
    display_2 = display->displays[2];

    /* Empty displays 0 and 1; each case below fills only the sprites it uses. */
    sprite_display_store_data(display_0, 0, 0, -32, 0, 0, 0, 0, 0);
    sprite_display_store_data(display_1, 0, 0, -32, 0, 0, 0, 0, 0);

    switch (display->selector & RESULT_DISPLAY_SELECTOR_MASK) {
    case RESULT_DISPLAY_HP_DAMAGE:
    case RESULT_DISPLAY_HP_HEALING:
    case RESULT_DISPLAY_MP_DAMAGE:
    case RESULT_DISPLAY_MP_HEALING:
        /* Damage (even selectors) shows at y 16, gains and healing at y -32. */
        y_shift = -32;
        if ((display->selector & 1) == 0) {
            y_shift = 16;
        }
        sprite_display_store_data(display_1, 0, -14, y_shift, 0, 0, 0, 0, 0);
        sprite_display_store_data(display_2, 0, -21, y_shift, 0, 0, 0, 0, 0);
        if ((digits[2] | digits[1]) == 0) {
            sprite_display_store_data(display_0, 0, -21, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
        } else if (digits[2] == 0) {
            sprite_display_store_data(display_0, 0, -14, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            sprite_display_store_data(display_1, 0, -21, y_shift, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
        } else {
            sprite_display_store_data(display_0, 0, -7, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            sprite_display_store_data(display_1, 0, -14, y_shift, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
            sprite_display_store_data(display_2, 0, -21, y_shift, 0xa8 + digits[2] * 8, 0x30, 8, 16, 0);
        }
        break;

    case RESULT_DISPLAY_SPEED_DAMAGE:
    case RESULT_DISPLAY_SPEED_GAIN:
    case RESULT_DISPLAY_CT_DAMAGE:
    case RESULT_DISPLAY_CT_GAIN:
    case RESULT_DISPLAY_BRAVE_DAMAGE:
    case RESULT_DISPLAY_BRAVE_GAIN:
    case RESULT_DISPLAY_FAITH_DAMAGE:
    case RESULT_DISPLAY_FAITH_GAIN:
    case RESULT_DISPLAY_PA_DAMAGE:
    case RESULT_DISPLAY_PA_GAIN:
    case RESULT_DISPLAY_MA_DAMAGE:
    case RESULT_DISPLAY_MA_GAIN:
        y_shift = -32;
        if ((display->selector & 1) == 0) {
            y_shift = 16;
        }
        if (digits[1] == 0) {
            sprite_display_store_data(display_0, 0, -14, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
        } else {
            sprite_display_store_data(display_0, 0, -14, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            sprite_display_store_data(display_1, 0, -21, y_shift, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
        }

        /* Masking bit 0 lets each damage case also label the matching gain. */
        switch (display->selector & 0x0e) {
        case RESULT_DISPLAY_SPEED_DAMAGE:
            sprite_display_store_data(display_2, 0, -7, y_shift, 0x9c, 0xc2, 0x18, 0x0a, 0);
            break;
        case RESULT_DISPLAY_CT_DAMAGE:
            sprite_display_store_data(display_2, 0, -7, y_shift, 0xc2, 0xc2, 0x0e, 0x0a, 0);
            break;
        case RESULT_DISPLAY_BRAVE_DAMAGE:
            sprite_display_store_data(display_2, 0, -7, y_shift, 0xa8, 0xb8, 0x0b, 0x0a, 0);
            break;
        case RESULT_DISPLAY_FAITH_DAMAGE:
            sprite_display_store_data(display_2, 0, -7, y_shift, 0x9c, 0xb8, 0x0c, 0x0a, 0);
            break;
        case RESULT_DISPLAY_PA_DAMAGE:
            sprite_display_store_data(display_2, 0, -7, y_shift, 0x40, 0, 0x0c, 0x0d, 0);
            break;
        case RESULT_DISPLAY_MA_DAMAGE:
            sprite_display_store_data(display_2, 0, -7, y_shift, 0x40, 0x0d, 0x0b, 0x0a, 0);
            break;
        }
        break;

    case RESULT_DISPLAY_MISSED:
        sprite_display_store_data(display_2, 0, -21, -32, 0xbe, 0x48, 0x1b, 8, 0);
        break;
    case RESULT_DISPLAY_GUARDED:
        sprite_display_store_data(display_2, 0, -21, -32, 0xb8, 0x60, 0x1e, 8, 0);
        break;
    case RESULT_DISPLAY_CAUGHT:
        sprite_display_store_data(display_2, 0, -21, -32, 0x30, 0xe6, 0x1a, 0x0a, 0);
        break;
    case RESULT_DISPLAY_STATUS_INFLICTED:
    case RESULT_DISPLAY_STATUS_REMOVED:
        image_id = status;
        sprite_display_store_data(display_2, 0, -21, -32, g_result_image_rects[image_id].u,
            g_result_image_rects[image_id].v, g_result_image_rects[image_id].width,
            g_result_image_rects[image_id].height, 0);
        break;
    case RESULT_DISPLAY_LEVEL_DOWN:
        /* The fall-through overwrites both sprites, so level down places them like level up. */
        sprite_display_store_data(display_1, 0, -7, 16, 0xb0, 0x30, 8, 16, 0);
        sprite_display_store_data(display_2, 0, 0, 16, 0xb4, 0xc1, 0x0e, 0x0a, 0);
        /* fall through */
    case RESULT_DISPLAY_LEVEL_UP:
        sprite_display_store_data(display_1, 0, -7, -32, 0xb0, 0x30, 8, 16, 0);
        sprite_display_store_data(display_2, 0, 0, -32, 0xb4, 0xc1, 0x0e, 0x0a, 0);
        break;
    case RESULT_DISPLAY_STOLEN:
        image = &g_result_image_rects[ACTION_DISPLAY_IMAGE_STOLEN];
        sprite_display_store_data(display_2, 0, -21, -32, image->u, image->v, image->width, image->height, 0);
        break;
    case RESULT_DISPLAY_BROKEN:
        image = &g_result_image_rects[ACTION_DISPLAY_IMAGE_BROKEN];
        sprite_display_store_data(display_2, 0, -21, -32, image->u, image->v, image->width, image->height, 0);
        break;
    case RESULT_DISPLAY_QUICK:
        image = &g_result_image_rects[ACTION_DISPLAY_IMAGE_QUICK];
        sprite_display_store_data(display_2, 0, -21, -32, image->u, image->v, image->width, image->height, 0);
        break;
    case RESULT_DISPLAY_CT_ZERO:
        /* y_shift is never set on this path; its stale value picks the row (QUIRKS.md). */
        sprite_display_store_data(display_1, 0, -16, y_shift, 0xc2, 0xc2, 0x0e, 8, 0);
        sprite_display_store_data(display_2, 0, 0, y_shift, 0xa8, 0x34, 8, 8, 0);
        break;
    }

    battle_map_palette_load_data(&g_result_palette, BATTLE_MAP_CLUT_ROW_ACTION_RESULTS, unit->runtime_id, 0);
    return 1;
}

/* Queues the post-action displays for a unit's resolved action.
 *
 * A hit sets one action_display_flags bit per HP/MP, stat, level, steal and
 * break result and lists the inflicted/removed status images (removals
 * marked 0x80); a miss queues MISSED, GUARDED or CAUGHT by miss type. The
 * first display is built immediately unless a numeric display is active. */
void result_display_prepare(unit_runtime_t* unit) {
    unit_stats_t* stats;
    action_data_t* action;
    s32 i;
    s32 j;
    s32 bit;
    u8 count;
    u8 image;
    u8 miss;

    unit->numeric_display.displays[0]->tpage = unit->numeric_display.displays[1]->tpage
        = unit->numeric_display.displays[2]->tpage = SPRITE_TPAGE_FRAME;
    stats = unit->stats;
    action = &stats->action;
    if (action->hit != 0) {
        if (action->attack_type & ACTION_TYPE_HP_DAMAGE) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_HP_DAMAGE;
        }
        if (action->attack_type & ACTION_TYPE_HP_HEALING) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_HP_HEALING;
        }
        if (action->attack_type & ACTION_TYPE_MP_DAMAGE) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_MP_DAMAGE;
        }
        if (action->attack_type & ACTION_TYPE_MP_HEALING) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_MP_HEALING;
        }
        if (action->attack_type & ACTION_TYPE_PSEUDO_STATUS) {
            if (action->sp_change != 0) {
                if (action->sp_change & ACTION_STAT_CHANGE_INCREASE) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_SPEED_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_SPEED_DAMAGE;
                }
            }
            if (action->ct_change != 0) {
                if (action->ct_change == ACTION_CT_CHANGE_QUICK) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_QUICK;
                } else if (action->ct_change == ACTION_CT_CHANGE_ZERO) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_CT_ZERO;
                } else if (action->ct_change & 0x80) { /* the top bit marks a gain, as for the stats */
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_CT_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_CT_DAMAGE;
                }
            }
            if (action->pa_change != 0) {
                if (action->pa_change & ACTION_STAT_CHANGE_INCREASE) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_PA_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_PA_DAMAGE;
                }
            }
            if (action->ma_change != 0) {
                if (action->ma_change & ACTION_STAT_CHANGE_INCREASE) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_MA_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_MA_DAMAGE;
                }
            }
            if (action->brave_change != 0) {
                if (action->brave_change & ACTION_STAT_CHANGE_INCREASE) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_BRAVE_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_BRAVE_DAMAGE;
                }
            }
            if (action->faith_change != 0) {
                if (action->faith_change & ACTION_STAT_CHANGE_INCREASE) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_FAITH_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_FAITH_DAMAGE;
                }
            }
        }
        if (action->special_effect & ACTION_SPECIAL_EFFECT_LEVEL_UP) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_LEVEL_UP;
        }
        if (action->special_effect & ACTION_SPECIAL_EFFECT_LEVEL_DOWN) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_LEVEL_DOWN;
        }
        if (action->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_STOLEN;
        }
        if (action->special_effect
            & (ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT | ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN)) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_BROKEN;
        }
        if (action->attack_type & ACTION_TYPE_STATUS_CHANGE) {
            unit->status_display_image_count = 0;
            for (i = 0; i < STATUS_BYTE_COUNT; i++) {
                for (j = 0; j < 8; j++) {
                    count = unit->status_display_image_count;
                    bit = 0x80 >> j;
                    if (count < ACTION_STATUS_DISPLAY_CAPACITY) {
                        if (action->status_changes.infliction[i] & bit) {
                            image = g_result_status_image_ids[i][j];
                            unit->status_display_image_list[count] = image;
                            if (image != 0) {
                                unit->status_display_image_count++;
                            }
                        } else if (action->status_changes.removal[i] & bit) {
                            image = g_result_status_image_ids[i][j];
                            unit->status_display_image_list[count] = image;
                            if (image != 0) {
                                unit->status_display_image_list[unit->status_display_image_count]
                                    |= ACTION_STATUS_DISPLAY_REMOVAL;
                                unit->status_display_image_count++;
                            }
                        }
                    }
                }
            }
        }
    } else {
        /* An evade (accessory, either hand, Blade Grasp or Finger Guard) or a forced
         * failure reads MISSED for an ability that targets allies only
         * (ACTION_MISS_DISPLAY_ALLIES_ONLY) and GUARDED otherwise; a catch reads
         * CAUGHT, a canceled or reflected action nothing, any other miss MISSED. */
        miss = action->miss_type;
        if (miss != ACTION_MISS_TYPE_HIT) {
            /* types 1 and 2: accessory and right-hand evade */
            if ((u32)(action->miss_type - 1) < 2 || miss == ACTION_MISS_TYPE_LEFT_HAND_EVADE
                || miss == ACTION_MISS_TYPE_FORCED_FAILURE || miss == ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD) {
                if ((action->miss_display_flags & ACTION_MISS_DISPLAY_TEAM_MASK) == ACTION_MISS_DISPLAY_ALLIES_ONLY) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_MISSED;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_GUARDED;
                }
            } else if (miss == ACTION_MISS_TYPE_CATCH) {
                unit->action_display_flags |= ACTION_DISPLAY_FLAG_CAUGHT;
            } else if (miss != ACTION_MISS_TYPE_CANCELED && miss != ACTION_MISS_TYPE_HIT
                && miss != ACTION_MISS_TYPE_REFLECTED) {
                unit->action_display_flags |= ACTION_DISPLAY_FLAG_MISSED;
            }
        }
    }
    if (unit->numeric_display.active == 0) {
        unit->numeric_display.progress = 0;
        result_next_display_build(unit);
    }
}

/* Builds the next EXP/JP or failure-message display: gained Exp, then gained JP,
 * then no target, silenced or not enough MP, clearing that flag. A number shows
 * its last two digits (a value over 999 shows 99).
 *
 * The companion of result_next_display_build for flags
 * 0x02000000 and above. Returns 0 when no flag is pending. Reading the
 * flags into a block-local before clearing keeps the target's and operand
 * order. */
s32 result_next_special_display_build(unit_runtime_t* unit) {
    action_result_display_t* display;
    sprite_display_t* display_0;
    sprite_display_t* display_1;
    sprite_display_t* display_2;
    s32 digits[3];
    u32 flags;
    u32 clear;
    u16 selector;

    flags = unit->action_display_flags;
    display = &unit->numeric_display;
    if ((flags & ACTION_DISPLAY_FLAG_GAINED_EXP) != 0) {
        unit->numeric_display.selector = RESULT_DISPLAY_GAINED_EXP;
        unit->numeric_display.active = 1;
        unit->numeric_display.value = unit->strike.action_rewards.earned_experience;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_GAINED_EXP;
        if (unit->numeric_display.progress >= 22) {
            unit->numeric_display.progress = 21;
        }
    } else if ((flags & ACTION_DISPLAY_FLAG_GAINED_JP) != 0) {
        unit->numeric_display.selector = RESULT_DISPLAY_GAINED_JP;
        unit->numeric_display.active = 1;
        unit->numeric_display.value = unit->strike.action_rewards.earned_jp;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_GAINED_JP;
        if (unit->numeric_display.progress >= 22) {
            unit->numeric_display.progress = 21;
        }
    } else {
        if ((flags & ACTION_DISPLAY_FLAG_NO_TARGET) != 0) {
            clear = ~ACTION_DISPLAY_FLAG_NO_TARGET;
            selector = RESULT_DISPLAY_NO_TARGET;
        } else if ((flags & ACTION_DISPLAY_FLAG_SILENCED) != 0) {
            clear = ~ACTION_DISPLAY_FLAG_SILENCED;
            selector = RESULT_DISPLAY_SILENCED;
        } else if ((flags & ACTION_DISPLAY_FLAG_NO_MP) != 0) {
            clear = ~ACTION_DISPLAY_FLAG_NO_MP;
            selector = RESULT_DISPLAY_NO_MP;
        } else {
            return 0;
        }
        unit->numeric_display.selector = selector;
        unit->numeric_display.active = 1;
        {
            u32 current;
            current = unit->action_display_flags;
            unit->action_display_flags = current & clear;
        }
        if (unit->numeric_display.progress >= 22) {
            unit->numeric_display.progress = 21;
        }
    }

    display->displays[0]->color.r = display->displays[1]->color.r = display->displays[2]->color.r = 150;
    display->displays[0]->color.g = display->displays[1]->color.g = display->displays[2]->color.g = 140;
    display->displays[0]->color.b = display->displays[1]->color.b = display->displays[2]->color.b = 20;

    if ((display->value / 1000) & 0xffff) {
        digits[2] = 9;
        digits[1] = 9;
        digits[0] = 9;
    } else {
        digits[0] = (display->value % 10) & 0xffff;
        digits[1] = (((display->value / 10) & 0xffff) % 10) & 0xffff;
        digits[2] = (display->value / 100) & 0xffff;
    }

    display_0 = display->displays[0];
    display_1 = display->displays[1];
    display_2 = display->displays[2];

    /* Empty displays 1 and 2; each case below fills only the sprites it uses. */
    sprite_display_store_data(display_2, 0, -21, 16, 0, 0, 0, 0, 0);
    sprite_display_store_data(display_1, 0, -21, 16, 0, 0, 0, 0, 0);

    switch (display->selector & RESULT_DISPLAY_SELECTOR_MASK) {
    case RESULT_DISPLAY_GAINED_EXP:
        if (digits[1] == 0) {
            sprite_display_store_data(display_0, 0, -7, -24, 0xc0, 0xb8, 0x12, 9, 0);
            sprite_display_store_data(display_1, 0, -14, -32, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
        } else {
            sprite_display_store_data(display_0, 0, -7, -24, 0xc0, 0xb8, 0x12, 9, 0);
            sprite_display_store_data(display_1, 0, -14, -32, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            sprite_display_store_data(display_2, 0, -21, -32, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
        }
        break;
    case RESULT_DISPLAY_GAINED_JP:
        if (digits[1] == 0) {
            sprite_display_store_data(display_0, 0, -7, -24, 0xb4, 0xb8, 0x0c, 9, 0);
            sprite_display_store_data(display_1, 0, -14, -32, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
        } else {
            sprite_display_store_data(display_0, 0, -7, -24, 0xb4, 0xb8, 0x0c, 9, 0);
            sprite_display_store_data(display_1, 0, -14, -32, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            sprite_display_store_data(display_2, 0, -21, -32, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
        }
        break;
    case RESULT_DISPLAY_NO_TARGET:
        display->displays[0]->color.r = display->displays[0]->color.g = display->displays[0]->color.b = 128;
        sprite_display_store_data(display_0, 0, -14, -24, 0x1a, 0xdb, 0x25, 10, 0);
        break;
    case RESULT_DISPLAY_SILENCED:
        display->displays[0]->color.r = display->displays[0]->color.g = display->displays[0]->color.b = 128;
        sprite_display_store_data(display_0, 0, -14, -24, 0xd8, 0xe9, 0x1e, 8, 0);
        break;
    case RESULT_DISPLAY_NO_MP:
        display->displays[0]->color.r = display->displays[0]->color.g = display->displays[0]->color.b = 128;
        sprite_display_store_data(display_0, 0, -14, -24, 0x64, 0x20, 0x1a, 9, 0);
        break;
    }

    battle_map_palette_load_data(&g_result_palette, BATTLE_MAP_CLUT_ROW_ACTION_RESULTS, unit->runtime_id, 0);
    return 1;
}

/* Queues the gained Exp and JP displays after an action, when the Display Gained
 * Exp/JP option is on and the amount is not 0. The first display is built at once
 * unless one is already running. */
void result_exp_jp_display_init(unit_runtime_t* unit) {
    sprite_display_t* display_0;
    sprite_display_t* display_1;
    sprite_display_t* display_2;

    if (g_game_options.fields.display_gained_exp_jp == GAME_OPTION_ON) {
        display_0 = unit->numeric_display.displays[0];
        display_1 = unit->numeric_display.displays[1];
        display_2 = unit->numeric_display.displays[2];
        display_2->tpage = SPRITE_TPAGE_FRAME;
        display_1->tpage = SPRITE_TPAGE_FRAME;
        display_0->tpage = SPRITE_TPAGE_FRAME;
        if (unit->strike.action_rewards.earned_experience != 0) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_GAINED_EXP;
        }
        if (unit->strike.action_rewards.earned_jp != 0) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_GAINED_JP;
        }
        if (unit->numeric_display.active == 0) {
            unit->numeric_display.progress = 0;
            result_next_special_display_build(unit);
        }
    }
}

/* Queues the failure message for a charged action that cannot start: mode is the
 * action_rejection_e of action_init_current_data, and only no target,
 * silenced and not enough MP queue one.
 *
 * The three numeric displays are pointed at the FRAME.BIN page, and the first
 * display is built at once unless one is already running. */
void result_charge_failure_queue(unit_runtime_t* unit, s32 mode) {
    sprite_display_t* display_0 = unit->numeric_display.displays[0];
    sprite_display_t* display_1 = unit->numeric_display.displays[1];
    sprite_display_t* display_2 = unit->numeric_display.displays[2];

    display_2->tpage = SPRITE_TPAGE_FRAME;
    display_1->tpage = SPRITE_TPAGE_FRAME;
    display_0->tpage = SPRITE_TPAGE_FRAME;
    switch (mode) {
    case ACTION_REJECTION_NO_TARGET:
        unit->action_display_flags |= ACTION_DISPLAY_FLAG_NO_TARGET;
        break;
    case ACTION_REJECTION_SILENCED:
        unit->action_display_flags |= ACTION_DISPLAY_FLAG_SILENCED;
        break;
    case ACTION_REJECTION_NOT_ENOUGH_MP:
        unit->action_display_flags |= ACTION_DISPLAY_FLAG_NO_MP;
        break;
    }
    if (unit->numeric_display.active == 0) {
        unit->numeric_display.progress = 0;
        result_next_special_display_build(unit);
    }
}

/* Animate and queue the three numeric post-action display sprites.
 *
 * Over the first 21 frames each digit sprite scales by its own zoom curve (the
 * critical curves for a critical hit); the word displays (Missed, Guarded, EXP,
 * JP) scale all three parts by the middle digit's curve. At frame 50 the next
 * queued result starts, or else the display begins fading: its palette darkens
 * and its parts turn semi-transparent. After frame 61 it retires unless another
 * result follows. Damage draws 0x30 pixels above the unit's screen position,
 * healing and gains 0x10, the words 8. */
void result_text_animate(unit_runtime_t* unit, const SVECTOR* screen_coords) {
    SVECTOR position;
    SVECTOR zoom_0 = g_identity_scale;
    SVECTOR zoom_1 = g_identity_scale;
    SVECTOR zoom_2 = g_identity_scale;
    s16 scale_0[21] = { 0, 1060, 2048, 2896, 3574, 3956, ONE, 3584, 3584, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE,
        ONE, ONE, ONE };
    s16 scale_1[21]
        = { 0, 0, 0, 0, 0, 0, 0, 1060, 2048, 2896, 3574, 3956, ONE, 3584, 3584, ONE, ONE, ONE, ONE, ONE, ONE };
    s16 scale_2[21] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1060, 2048, 2896, 3574, 3956, ONE, 3584, 3584 };
    s16 critical_scale_0[21] = { 0, 1228, 2457, 3685, 4913, 6144, 4913, 3685, 3900, ONE, ONE, ONE, ONE, ONE, ONE, ONE,
        ONE, ONE, ONE, ONE, ONE };
    s16 critical_scale_1[21]
        = { 0, 0, 0, 0, 0, 0, 0, 1228, 2457, 3685, 4913, 6144, 4913, 3685, 3900, ONE, ONE, ONE, ONE, ONE, ONE };
    s16 critical_scale_2[21]
        = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1228, 2457, 3685, 4913, 6144, 4913, 3685, 3900, ONE };
    sprite_display_t* display_0;
    sprite_display_t* display_1;
    sprite_display_t* display_2;
    action_result_display_t* display;
    u16 progress;

    display_0 = unit->numeric_display.displays[0];
    display_1 = unit->numeric_display.displays[1];
    display_2 = unit->numeric_display.displays[2];
    progress = unit->numeric_display.progress + g_system_vsync_interval;
    display = &unit->numeric_display;
    if (progress < 21) {
        if (unit->numeric_display.selector & RESULT_DISPLAY_MODIFIER_CRITICAL) {
            vector_short_set(
                &zoom_0, critical_scale_0[progress], critical_scale_0[progress], critical_scale_0[progress]);
            vector_short_set(
                &zoom_1, critical_scale_1[progress], critical_scale_1[progress], critical_scale_1[progress]);
            vector_short_set(
                &zoom_2, critical_scale_2[progress], critical_scale_2[progress], critical_scale_2[progress]);
        } else {
            switch (unit->numeric_display.selector & RESULT_DISPLAY_SELECTOR_MASK) {
            case RESULT_DISPLAY_HP_DAMAGE ... RESULT_DISPLAY_MA_GAIN:
                vector_short_set(&zoom_0, scale_0[progress], scale_0[progress], scale_0[progress]);
                vector_short_set(&zoom_1, scale_1[progress], scale_1[progress], scale_1[progress]);
                vector_short_set(&zoom_2, scale_2[progress], scale_2[progress], scale_2[progress]);
                break;
            case RESULT_DISPLAY_MISSED:
            case RESULT_DISPLAY_GUARDED:
            case RESULT_DISPLAY_GAINED_EXP:
            case RESULT_DISPLAY_GAINED_JP:
                vector_short_set(&zoom_0, scale_1[progress], scale_1[progress], scale_1[progress]);
                vector_short_set(&zoom_1, scale_1[progress], scale_1[progress], scale_1[progress]);
                vector_short_set(&zoom_2, scale_1[progress], scale_1[progress], scale_1[progress]);
                break;
            }
        }
    } else if (progress >= 50 && unit->numeric_display.progress < 50) {
        if (result_next_display_build(unit) != 0) {
            return;
        }
        if (result_next_special_display_build(unit) != 0) {
            return;
        }
        unit->numeric_display.displays[0]->tpage = unit->numeric_display.displays[1]->tpage
            = unit->numeric_display.displays[2]->tpage = SPRITE_TPAGE_FRAME | SPRITE_DISPLAY_BLEND_ADD;
        battle_map_palette_modify(
            BATTLE_MAP_COLOR_MODE_BASE, 2, BATTLE_MAP_CLUT_ROW_ACTION_RESULTS, unit->runtime_id, 0, -31, -31, -31);
        sprite_display_set_part_semi_transparent(display_0, 0, 1);
        sprite_display_set_part_semi_transparent(display_1, 0, 1);
        sprite_display_set_part_semi_transparent(display_2, 0, 1);
    } else if (progress >= 61) {
        if (result_next_display_build(unit) != 0) {
            return;
        }
        if (result_next_special_display_build(unit) != 0) {
            return;
        }
        unit->numeric_display.active = 0;
    }
    position.vx = screen_coords->vx + 0x15;
    position.vy = screen_coords->vy - 8;
    switch (display->selector & RESULT_DISPLAY_SELECTOR_MASK) {
    case RESULT_DISPLAY_HP_DAMAGE:
    case RESULT_DISPLAY_MP_DAMAGE:
    case RESULT_DISPLAY_SPEED_DAMAGE:
    case RESULT_DISPLAY_CT_DAMAGE:
    case RESULT_DISPLAY_BRAVE_DAMAGE:
    case RESULT_DISPLAY_FAITH_DAMAGE:
    case RESULT_DISPLAY_PA_DAMAGE:
    case RESULT_DISPLAY_MA_DAMAGE:
        position.vy = screen_coords->vy - 0x30;
        break;
    case RESULT_DISPLAY_HP_HEALING:
    case RESULT_DISPLAY_MP_HEALING:
    case RESULT_DISPLAY_SPEED_GAIN:
    case RESULT_DISPLAY_CT_GAIN:
    case RESULT_DISPLAY_BRAVE_GAIN:
    case RESULT_DISPLAY_FAITH_GAIN:
    case RESULT_DISPLAY_PA_GAIN:
    case RESULT_DISPLAY_MA_GAIN:
        position.vy = screen_coords->vy - 0x10;
        break;
    case RESULT_DISPLAY_MISSED:
    case RESULT_DISPLAY_GUARDED:
    case RESULT_DISPLAY_GAINED_EXP:
    case RESULT_DISPLAY_GAINED_JP:
        break;
    }
    sprite_display_construct_polygon_data(
        display_0, display_0->part_count, 0, &position, 0, 0, &zoom_0, gfx_battle_otag_get() + 1);
    sprite_display_construct_polygon_data(
        display_1, display_1->part_count, 0, &position, 0, 0, &zoom_1, gfx_battle_otag_get() + 1);
    sprite_display_construct_polygon_data(
        display_2, display_2->part_count, 0, &position, 0, 0, &zoom_2, gfx_battle_otag_get() + 1);
    display->progress += g_system_vsync_interval;
}
