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
 * pieces (menu_gfx_enqueue_oriented_textured_quad modes 0..3) from two sprite layers at OT 3 and 4.
 * Compiled at -O1 like the other formation sprite drawers; the mix of direct,
 * struct-pointer and s16-pointer accesses to the newest point selects the
 * target's address registers.
 */
void world_formation_draw_cursor_trail(point16_t* cursor) {
    rgb24_t rgb;
    s32 i;
    s32 delta;
    s32 sum;
    s32 previous_sum;
    rgb24_t* color;
    s16* trail_y;
    point16_t* newest;

    for (i = 0; i < 7; i++) {
        g_world_formation_cursor_trail[i].x = g_world_formation_cursor_trail[i + 1].x;
        g_world_formation_cursor_trail[i].y = g_world_formation_cursor_trail[i + 1].y;
    }
    newest = &g_world_formation_cursor_trail[7];
    if (newest->x == -1) {
        g_world_formation_cursor_trail[7].x = cursor->x - 31;
        g_world_formation_cursor_trail[7].y = cursor->y - 1;
    } else {
        i = newest->x;
        newest->x = (i * 3 + (cursor->x - 31)) >> 2;
        delta = newest->x;
        if (delta < 0) {
            delta = -delta;
        }
        if (i == delta) {
            newest->x = cursor->x - 31;
        }
        trail_y = &g_world_formation_cursor_trail[7].y;
        i = *trail_y;
        *trail_y = (i * 3 + (cursor->y - 1)) >> 2;
        delta = *trail_y;
        if (delta < 0) {
            delta = -delta;
        }
        if (i == delta) {
            *trail_y = cursor->y - 1;
        }
    }
    previous_sum = 1000;
    for (i = 7; i >= 0; i--) {
        rgb.r = rgb.g = rgb.b = g_world_formation_cursor_trail_brightness[i];
        sum = g_world_formation_cursor_trail[i].x + g_world_formation_cursor_trail[i].y;
        delta = sum - previous_sum;
        if (delta < 0) {
            delta = -delta;
        }
        if (delta >= 5) {
            previous_sum = sum;
            color = 0;
            if (i != 7) {
                color = &rgb;
            }
            if (g_world_formation_cursor_trail[i].x != -1) {
                g_world_formation_cursor_sprite.position.x = g_world_formation_cursor_shadow_sprite.position.x
                    = g_world_formation_cursor_trail[i].x;
                g_world_formation_cursor_sprite.h = g_world_formation_cursor_sprite.source.h
                    = g_world_formation_cursor_shadow_sprite.h = g_world_formation_cursor_shadow_sprite.source.h = 16;
                g_world_formation_cursor_shadow_sprite.position.y = g_world_formation_cursor_trail[i].y;
                g_world_formation_cursor_sprite.position.y = g_world_formation_cursor_trail[i].y - 1;
                menu_gfx_enqueue_oriented_textured_quad(
                    &g_world_formation_cursor_shadow_sprite, color, TEXTURE_UV_FLIP_NONE, 1, 3);
                menu_gfx_enqueue_oriented_textured_quad(
                    &g_world_formation_cursor_sprite, color, TEXTURE_UV_FLIP_NONE, 1, 4);
                g_world_formation_cursor_shadow_sprite.position.x += 31;
                g_world_formation_cursor_sprite.position.x += 31;
                menu_gfx_enqueue_oriented_textured_quad(
                    &g_world_formation_cursor_shadow_sprite, color, TEXTURE_UV_FLIP_X, 1, 3);
                menu_gfx_enqueue_oriented_textured_quad(
                    &g_world_formation_cursor_sprite, color, TEXTURE_UV_FLIP_X, 1, 4);
                g_world_formation_cursor_sprite.position.x = g_world_formation_cursor_shadow_sprite.position.x
                    = g_world_formation_cursor_trail[i].x;
                g_world_formation_cursor_sprite.h = g_world_formation_cursor_sprite.source.h
                    = g_world_formation_cursor_shadow_sprite.h = g_world_formation_cursor_shadow_sprite.source.h = 15;
                g_world_formation_cursor_shadow_sprite.position.y = g_world_formation_cursor_trail[i].y + 16;
                g_world_formation_cursor_sprite.position.y = g_world_formation_cursor_trail[i].y + 15;
                menu_gfx_enqueue_oriented_textured_quad(
                    &g_world_formation_cursor_shadow_sprite, color, TEXTURE_UV_FLIP_Y, 1, 3);
                menu_gfx_enqueue_oriented_textured_quad(
                    &g_world_formation_cursor_sprite, color, TEXTURE_UV_FLIP_Y, 1, 4);
                g_world_formation_cursor_shadow_sprite.position.x += 31;
                g_world_formation_cursor_sprite.position.x += 31;
                menu_gfx_enqueue_oriented_textured_quad(
                    &g_world_formation_cursor_shadow_sprite, color, TEXTURE_UV_FLIP_XY, 1, 3);
                menu_gfx_enqueue_oriented_textured_quad(
                    &g_world_formation_cursor_sprite, color, TEXTURE_UV_FLIP_XY, 1, 4);
            }
        }
    }
}
