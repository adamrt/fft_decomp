#include "fft/world.h"
#include "psx/types.h"

/*
 * Shift the formation cursor trail history, ease its newest point toward the
 * cursor, and draw the visible trail frames from newest to oldest.
 *
 * Each coordinate moves a quarter of the way per frame and snaps to the
 * target once the step stops changing it. A point is drawn only when its x+y
 * differs from the last drawn point by at least 5; older points are tinted
 * with the g_world_formation_cursor_trail_brightness brightness ramp. Each frame is drawn as four quadrant
 * pieces (world_gfx_enqueue_oriented_textured_quad modes 0..3) from two sprite layers at OT 3 and 4.
 * Compiled at -O1 like the other formation sprite drawers; the mix of direct,
 * struct-pointer and s16-pointer accesses to the newest point selects the
 * target's address registers.
 */
void world_formation_draw_cursor_trail(s16* cursor) {
    u8 rgb[3];
    s32 i;
    s32 delta;
    s32 sum;
    s32 previous_sum;
    u8* color;
    s16* trail_y;
    world_cursor_trail_point_t* newest;

    for (i = 0; i < 7; i++) {
        g_world_formation_cursor_trail[i].x = g_world_formation_cursor_trail[i + 1].x;
        g_world_formation_cursor_trail[i].y = g_world_formation_cursor_trail[i + 1].y;
    }
    newest = &g_world_formation_cursor_trail[7];
    if (newest->x == -1) {
        g_world_formation_cursor_trail[7].x = cursor[0] - 31;
        g_world_formation_cursor_trail[7].y = cursor[1] - 1;
    } else {
        i = newest->x;
        newest->x = (i * 3 + (cursor[0] - 31)) >> 2;
        delta = newest->x;
        if (delta < 0) {
            delta = -delta;
        }
        if (i == delta) {
            newest->x = cursor[0] - 31;
        }
        trail_y = &g_world_formation_cursor_trail[7].y;
        i = *trail_y;
        *trail_y = (i * 3 + (cursor[1] - 1)) >> 2;
        delta = *trail_y;
        if (delta < 0) {
            delta = -delta;
        }
        if (i == delta) {
            *trail_y = cursor[1] - 1;
        }
    }
    previous_sum = 1000;
    for (i = 7; i >= 0; i--) {
        rgb[0] = rgb[1] = rgb[2] = g_world_formation_cursor_trail_brightness[i];
        sum = g_world_formation_cursor_trail[i].x + g_world_formation_cursor_trail[i].y;
        delta = sum - previous_sum;
        if (delta < 0) {
            delta = -delta;
        }
        if (delta >= 5) {
            previous_sum = sum;
            color = 0;
            if (i != 7) {
                color = rgb;
            }
            if (g_world_formation_cursor_trail[i].x != -1) {
                g_world_formation_cursor_sprite.x = g_world_formation_cursor_shadow_sprite.x
                    = g_world_formation_cursor_trail[i].x;
                g_world_formation_cursor_sprite.h = g_world_formation_cursor_sprite.v_h
                    = g_world_formation_cursor_shadow_sprite.h = g_world_formation_cursor_shadow_sprite.v_h = 16;
                g_world_formation_cursor_shadow_sprite.y = g_world_formation_cursor_trail[i].y;
                g_world_formation_cursor_sprite.y = g_world_formation_cursor_trail[i].y - 1;
                world_gfx_enqueue_oriented_textured_quad(
                    (const world_oriented_quad_t*)&g_world_formation_cursor_shadow_sprite, (const u8*)color, 0, 1, 3);
                world_gfx_enqueue_oriented_textured_quad(
                    (const world_oriented_quad_t*)&g_world_formation_cursor_sprite, (const u8*)color, 0, 1, 4);
                g_world_formation_cursor_shadow_sprite.x += 31;
                g_world_formation_cursor_sprite.x += 31;
                world_gfx_enqueue_oriented_textured_quad(
                    (const world_oriented_quad_t*)&g_world_formation_cursor_shadow_sprite, (const u8*)color, 1, 1, 3);
                world_gfx_enqueue_oriented_textured_quad(
                    (const world_oriented_quad_t*)&g_world_formation_cursor_sprite, (const u8*)color, 1, 1, 4);
                g_world_formation_cursor_sprite.x = g_world_formation_cursor_shadow_sprite.x
                    = g_world_formation_cursor_trail[i].x;
                g_world_formation_cursor_sprite.h = g_world_formation_cursor_sprite.v_h
                    = g_world_formation_cursor_shadow_sprite.h = g_world_formation_cursor_shadow_sprite.v_h = 15;
                g_world_formation_cursor_shadow_sprite.y = g_world_formation_cursor_trail[i].y + 16;
                g_world_formation_cursor_sprite.y = g_world_formation_cursor_trail[i].y + 15;
                world_gfx_enqueue_oriented_textured_quad(
                    (const world_oriented_quad_t*)&g_world_formation_cursor_shadow_sprite, (const u8*)color, 2, 1, 3);
                world_gfx_enqueue_oriented_textured_quad(
                    (const world_oriented_quad_t*)&g_world_formation_cursor_sprite, (const u8*)color, 2, 1, 4);
                g_world_formation_cursor_shadow_sprite.x += 31;
                g_world_formation_cursor_sprite.x += 31;
                world_gfx_enqueue_oriented_textured_quad(
                    (const world_oriented_quad_t*)&g_world_formation_cursor_shadow_sprite, (const u8*)color, 3, 1, 3);
                world_gfx_enqueue_oriented_textured_quad(
                    (const world_oriented_quad_t*)&g_world_formation_cursor_sprite, (const u8*)color, 3, 1, 4);
            }
        }
    }
}
