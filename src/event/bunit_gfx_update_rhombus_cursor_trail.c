#include "fft/event_bunit.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Advance the rhombus-cursor trail toward target and draw it.
 *
 * The newest point eases a quarter of the way toward target each frame and
 * snaps to it when the eased value's magnitude equals the previous value.
 * Each point far enough from the last drawn one is drawn as four oriented
 * quads per cursor layer, faded by g_bunit_cursor_trail_brightness except for the newest point.
 * abs() matters: GCC expands it to the single-insn abssi2, which keeps each
 * loop body one basic block as in the target. */
void bunit_gfx_update_rhombus_cursor_trail(bunit_cursor_trail_point_t* target) {
    u8 rgb[3];
    s32 i;
    s32 last;
    s32 sum;
    s16 x;
    s16 x2;
    u8* color;

    for (i = 0; i < 7;) {
        s32 j = i + 1;
        g_bunit_cursor_trail_points[i].x = g_bunit_cursor_trail_points[j].x;
        g_bunit_cursor_trail_points[i].y = g_bunit_cursor_trail_points[j].y;
        i = j;
    }
    if (g_bunit_cursor_trail_points[7].x == -1) {
        g_bunit_cursor_trail_points[7].x = target->x - 0x1F;
        g_bunit_cursor_trail_points[7].y = target->y - 1;
    } else {
        /* The (c - target) grouping keeps the constant on i * 3, as in the
         * target; (i * 3 - c + target) would be reassociated by fold. */
        i = g_bunit_cursor_trail_points[7].x;
        g_bunit_cursor_trail_points[7].x = (i * 3 - (0x1F - target->x)) >> 2;
        if (i == abs(g_bunit_cursor_trail_points[7].x)) {
            g_bunit_cursor_trail_points[7].x = target->x - 0x1F;
        }
        i = g_bunit_cursor_trail_points[7].y;
        g_bunit_cursor_trail_points[7].y = (i * 3 - (1 - target->y)) >> 2;
        if (i == abs(g_bunit_cursor_trail_points[7].y)) {
            g_bunit_cursor_trail_points[7].y = target->y - 1;
        }
    }
    last = 1000;
    for (i = 7; i >= 0; i--) {
        rgb[0] = rgb[1] = rgb[2] = g_bunit_cursor_trail_brightness[i];
        x = g_bunit_cursor_trail_points[i].x;
        sum = x + g_bunit_cursor_trail_points[i].y;
        if (abs(sum - last) < 5) {
            continue;
        }
        last = sum;
        color = 0;
        if (i != 7) {
            color = rgb;
        }
        if (x == -1) {
            continue;
        }
        g_bunit_cursor_sprite.x = g_bunit_cursor_shadow_sprite.x = x;
        g_bunit_cursor_sprite.h = g_bunit_cursor_sprite.vh = g_bunit_cursor_shadow_sprite.h
            = g_bunit_cursor_shadow_sprite.vh = 0x10;
        g_bunit_cursor_shadow_sprite.y = g_bunit_cursor_trail_points[i].y;
        g_bunit_cursor_sprite.y = g_bunit_cursor_trail_points[i].y - 1;
        bunit_gfx_enqueue_oriented_textured_quad(&g_bunit_cursor_shadow_sprite, color, 0, 1, 3);
        bunit_gfx_enqueue_oriented_textured_quad(&g_bunit_cursor_sprite, color, 0, 1, 4);
        g_bunit_cursor_shadow_sprite.x += 0x1F;
        g_bunit_cursor_sprite.x += 0x1F;
        bunit_gfx_enqueue_oriented_textured_quad(&g_bunit_cursor_shadow_sprite, color, 1, 1, 3);
        bunit_gfx_enqueue_oriented_textured_quad(&g_bunit_cursor_sprite, color, 1, 1, 4);
        x2 = g_bunit_cursor_trail_points[i].x;
        g_bunit_cursor_sprite.h = g_bunit_cursor_sprite.vh = g_bunit_cursor_shadow_sprite.h
            = g_bunit_cursor_shadow_sprite.vh = 0xF;
        g_bunit_cursor_sprite.x = g_bunit_cursor_shadow_sprite.x = x2;
        g_bunit_cursor_shadow_sprite.y = g_bunit_cursor_trail_points[i].y + 0x10;
        g_bunit_cursor_sprite.y = g_bunit_cursor_trail_points[i].y + 0xF;
        bunit_gfx_enqueue_oriented_textured_quad(&g_bunit_cursor_shadow_sprite, color, 2, 1, 3);
        bunit_gfx_enqueue_oriented_textured_quad(&g_bunit_cursor_sprite, color, 2, 1, 4);
        g_bunit_cursor_shadow_sprite.x += 0x1F;
        g_bunit_cursor_sprite.x += 0x1F;
        bunit_gfx_enqueue_oriented_textured_quad(&g_bunit_cursor_shadow_sprite, color, 3, 1, 3);
        bunit_gfx_enqueue_oriented_textured_quad(&g_bunit_cursor_sprite, color, 3, 1, 4);
    }
}
