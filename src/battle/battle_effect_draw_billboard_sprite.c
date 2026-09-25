#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Sprite frame table 4 bytes into g_battle_effect_trap_frame_data
 * (0x801b7694): the leading halfword is the offset of the per-frame offset
 * array, and each of those offsets reaches a frame record from the same base. */
#define SPRITE_TABLE (&g_battle_effect_trap_frame_data[4])

/* Queues one effect slot's billboard sprite: projects the slot origin for its
 * ordering-table depth, builds the rotated, camera-scaled 2D matrix from the
 * slot angle, transforms the frame's four corners into the slot's
 * double-buffered POLY_FT4, and adds that quad between the slot's two draw-mode
 * primitives. */
void battle_effect_draw_billboard_sprite(battle_effect_sprite_slot_t* slot) {
    SVECTOR local;
    VECTOR position;
    s32 sxy;
    s32 flag;
    battle_effect_quad_prim_t* prim;
    battle_effect_sprite_frame_t* frame;
    u16 offset;
    u32 color;
    /* Reusing v1 preserves the packed-color store before the opcode load. */
    register s32 code __asm__("$3");
    s32 depth;
    s32 sine;
    s32 cosine;
    s32 zoom;
    s32 scale_y;
    u8 corner_u;
    u8 corner_v;
    u8 right;
    u8 bottom;

    SetRotMatrix(&g_battle_camera_matrix);
    SetTransMatrix(&g_battle_camera_matrix);
    local.vx = slot->x >> 12;
    local.vy = slot->y >> 12;
    local.vz = slot->z >> 12;
    RotTrans(&local, &position, (long*)&position.pad);
    depth = (position.vz >> 2) - 8;
    if (depth < EFFECT_OT_DEPTH_MIN) {
        depth = EFFECT_OT_DEPTH_MIN;
    }
    if (depth >= EFFECT_OT_DEPTH_LIMIT) {
        depth = EFFECT_OT_DEPTH_MAX;
    }
    g_battle_effect_matrix.t[0] = position.vx;
    g_battle_effect_matrix.t[1] = position.vy;
    cosine = rcos(slot->angle);
    sine = rsin(slot->angle);
    zoom = g_battle_camera_zoom.vx;
    scale_y = g_battle_camera_zoom.vy;
    g_battle_effect_matrix.m[0][0] = (cosine * zoom) >> 12;
    g_battle_effect_matrix.m[1][1] = (cosine * scale_y) >> 12;
    g_battle_effect_matrix.m[0][1] = (-sine * scale_y) >> 12;
    g_battle_effect_matrix.m[1][0] = (sine * zoom) >> 12;
    SetRotMatrix(&g_battle_effect_matrix);
    SetTransMatrix(&g_battle_effect_matrix);

    offset = *(u16*)&SPRITE_TABLE[slot->color.field.frame * 2
        + ((battle_effect_sprite_table_header_t*)SPRITE_TABLE)->offsets_offset];
    prim = &slot->quads[g_battle_effect_buffer_index];
    color = slot->color.packed;
    prim->length = 9;
    prim->color.packed = color;
    code = 0x2c;
    prim->color.field.cd = code;
    frame = (battle_effect_sprite_frame_t*)&SPRITE_TABLE[offset];
    if (frame->flags & 0x200) {
        prim->color.field.cd = 0x2e;
    }
    /* Keeps the &prim->xy0 setup ($a3) after this branch; otherwise GCC hoists
     * it into the branch delay slot. */
    __asm__("" : : : "$7");
    ((battle_effect_corner_t*)&g_battle_effect_corner_top_left)->packed = frame->corners[0];
    ((battle_effect_corner_t*)&g_battle_effect_corner_top_right)->packed = frame->corners[1];
    ((battle_effect_corner_t*)&g_battle_effect_corner_bottom_left)->packed = frame->corners[2];
    ((battle_effect_corner_t*)&g_battle_effect_corner_bottom_right)->packed = frame->corners[3];
    RotTransPers3(&g_battle_effect_corner_top_left, &g_battle_effect_corner_top_right,
        &g_battle_effect_corner_bottom_left, &prim->xy0, &prim->xy1, &prim->xy2, &sxy, &flag);
    RotTransPers(&g_battle_effect_corner_bottom_right, &prim->xy3, &sxy, &flag);
    prim->tpage = (frame->flags & 0xe0) | 8;
    prim->clut = slot->clut;
    corner_u = frame->u;
    prim->u2 = corner_u;
    prim->u0 = corner_u;
    right = corner_u + frame->width;
    prim->u3 = right;
    prim->u1 = right;
    corner_v = frame->v;
    prim->v1 = corner_v;
    prim->v0 = corner_v;
    bottom = corner_v + frame->height;
    prim->v3 = bottom;
    prim->v2 = bottom;
    SetDrawMode(&slot->modes[g_battle_effect_buffer_index][0], 1, 1, 1, 0);
    AddPrim(&g_battle_effect_polygon_depth_data[depth], &slot->modes[g_battle_effect_buffer_index][0]);
    AddPrim(&g_battle_effect_polygon_depth_data[depth], prim);
    SetDrawMode(&slot->modes[g_battle_effect_buffer_index][1], 1, 0, 1, 0);
    AddPrim(&g_battle_effect_polygon_depth_data[depth], &slot->modes[g_battle_effect_buffer_index][1]);
}
