#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/*
 * Queues the item-drop sprite for the current secondary effect: projects the
 * effect origin to find its OT depth, builds a rotated and camera-scaled 2D
 * matrix from the sprite angle, and transforms the four quad corners into the
 * slot's double-buffered primitive before adding it to the ordering table.
 */
void effect_draw_item_drop_sprite(VECTOR* origin, s32 item_id, s32 angle) {
    SPRT sprite;
    SVECTOR local;
    VECTOR position;
    s32 sxy;
    s32 flag;
    effect_slot_t* slot;
    POLY_FT4* prim;
    s32 depth;
    s32 sine;
    s32 cosine;
    s32 zoom;
    s32 scale_y;
    s32 half_w;
    s32 half_h;
    s16 right;
    s16 bottom;

    slot = &g_effect_slots[g_effect_current_secondary->slot_ids[0]];
    gfx_set_item_icon_sprite(&sprite, item_id);
    prim = &slot->quads[g_effect_buffer_index];
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    copyVector(&local, origin);
    RotTrans(&local, &position, &position.pad);
    depth = (position.vz >> 2) - 8;
    if (depth < EFFECT_OT_DEPTH_MIN) {
        depth = EFFECT_OT_DEPTH_MIN;
    }
    if (depth >= EFFECT_OT_DEPTH_LIMIT) {
        depth = EFFECT_OT_DEPTH_MAX;
    }
    g_effect_matrix.t[0] = position.vx;
    g_effect_matrix.t[1] = position.vy;
    cosine = rcos(angle);
    sine = rsin(angle);
    zoom = g_camera_zoom.vx;
    scale_y = g_camera_zoom.vy;
    g_effect_matrix.m[0][0] = (cosine * zoom) >> 12;
    g_effect_matrix.m[1][1] = (cosine * scale_y) >> 12;
    g_effect_matrix.m[0][1] = (-sine * scale_y) >> 12;
    g_effect_matrix.m[1][0] = (sine * zoom) >> 12;
    SetRotMatrix(&g_effect_matrix);
    SetTransMatrix(&g_effect_matrix);
    half_w = -sprite.w / 2;
    right = half_w + sprite.w;
    half_h = -sprite.h / 2;
    bottom = half_h + sprite.h;
    g_effect_corner_bottom_left.vx = half_w;
    g_effect_corner_top_left.vx = half_w;
    g_effect_corner_bottom_right.vx = right;
    g_effect_corner_top_right.vx = right;
    g_effect_corner_top_right.vy = half_h;
    g_effect_corner_top_left.vy = half_h;
    g_effect_corner_bottom_right.vy = bottom;
    g_effect_corner_bottom_left.vy = bottom;
    RotTransPers3(&g_effect_corner_top_left, &g_effect_corner_top_right, &g_effect_corner_bottom_left, (s32*)&prim->x0,
        (s32*)&prim->x1, (s32*)&prim->x2, &sxy, &flag);
    RotTransPers(&g_effect_corner_bottom_right, (s32*)&prim->x3, &sxy, &flag);
    AddPrim(g_effect_polygon_depth_data + depth, prim);
}
