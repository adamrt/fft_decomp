#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Per-sprite height table, one byte every four. */

/*
 * Queue the map selection cursor over the currently selected tile.
 *
 * The explicit default arm is required: it gives GCC the target's jump past
 * the default 0x3c path. Scheduling the r0 color assignment directly after
 * u2 retains the primitive's semantics and produces the nearer late move.
 *
 * The two initialized locals occupy 64 bytes of .rodata at 0x80067798: the
 * eight-frame vertical bob offsets and their per-frame durations. The bob
 * advances once per call unless the status screen owns the frame. The cursor
 * floats 0x3c above the tile, or above the occupying unit's sprite height
 * scaled by the camera zoom, and its quad is sized from the camera zoom.
 */
void battle_gfx_draw_map_selection_cursor(SVECTOR* base, u32* ot) {
    VECTOR transformed;
    SVECTOR screen;
    s32 bob[8] = { 0, 1, 2, 3, 5, 3, 2, 1 };
    s32 duration[8] = { 16, 8, 2, 2, 6, 4, 4, 10 };
    long flag;
    POLY_FT4* prim;
    battle_unit_misc_data_t* unit;
    s32 height;
    s32 scaled;
    s32 half_width;
    s32 half_height;
    u8 color;

    prim = (POLY_FT4*)g_battle_unit_last_misc_init_byte + battle_gfx_increment_counter();
    ((P_TAG*)prim)->len = 9;
    prim->code = 0x2c;
    if (g_battle_menu_status_screen_selected != 1) {
        g_battle_gfx_selection_cursor_bob_timer += g_animation_speed;
        if (g_battle_gfx_selection_cursor_bob_timer >= duration[g_battle_gfx_selection_cursor_bob_frame]) {
            g_battle_gfx_selection_cursor_bob_frame += 1;
            g_battle_gfx_selection_cursor_bob_timer = 0;
            if (g_battle_gfx_selection_cursor_bob_frame >= 8) {
                g_battle_gfx_selection_cursor_bob_frame = 0;
            }
        }
    }
    unit = battle_unit_get_selectable_misc_data_at_map_coords(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (unit != 0) {
        scaled = g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height * g_battle_camera_zoom.vy;
        if (scaled < 0) {
            scaled += 0xfff;
        }
        height = (scaled >> 12) + 0x18;
    } else {
        height = 0x3c;
    }
    base->vy -= height - *(u16*)&bob[g_battle_gfx_selection_cursor_bob_frame];
    RotTrans(base, &transformed, &flag);

    half_width = g_battle_camera_zoom.vx;
    if (half_width < 0) {
        half_width += 0x1ff;
    }
    prim->x0 = transformed.vx - (half_width >> 9);
    {
        s32 zoom_height = g_battle_camera_zoom.vy * 12;
        if (zoom_height < 0) {
            zoom_height += 0xfff;
        }
        prim->y0 = transformed.vy - (zoom_height >> 12);
    }
    half_width = g_battle_camera_zoom.vx;
    if (half_width < 0) {
        half_width += 0x1ff;
    }
    prim->x1 = transformed.vx + (half_width >> 9);
    {
        s32 zoom_height = g_battle_camera_zoom.vy * 12;
        if (zoom_height < 0) {
            zoom_height += 0xfff;
        }
        prim->y1 = transformed.vy - (zoom_height >> 12);
    }
    half_width = g_battle_camera_zoom.vx;
    if (half_width < 0) {
        half_width += 0x1ff;
    }
    prim->x2 = transformed.vx - (half_width >> 9);
    {
        s32 zoom_height = g_battle_camera_zoom.vy * 12;
        if (zoom_height < 0) {
            zoom_height += 0xfff;
        }
        prim->y2 = transformed.vy + (zoom_height >> 12);
    }
    half_width = g_battle_camera_zoom.vx;
    if (half_width < 0) {
        half_width += 0x1ff;
    }
    prim->x3 = transformed.vx + (half_width >> 9);
    half_height = g_battle_camera_zoom.vy * 12;
    if (half_height < 0) {
        half_height += 0xfff;
    }
    {
        u16 screen_y = transformed.vy;
        prim->tpage = 0x5f;
        prim->y3 = screen_y + (half_height >> 12);
    }
    prim->code |= 2;
    color = 0x80;
    prim->clut = GetClut(0x40, 0x1e2);
    prim->r0 = prim->g0 = prim->b0 = color;
    prim->u2 = 0x40;
    prim->u0 = 0x40;
    prim->u3 = 0x50;
    prim->u1 = 0x50;
    prim->v1 = color;
    prim->v0 = color;
    prim->v3 = 0x98;
    prim->v2 = 0x98;
    battle_camera_convert_screen_coords_modify_by_1(&base->vx, &screen.vx);
    RotTrans(&screen, &transformed, &flag);
    AddPrim(ot + (transformed.vz / 4), prim);
}
