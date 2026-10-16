#include "fft/event_bunit.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Advances the animated cursor at `pos` and draws it, bouncing vertically,
 * with its semi-transparent shadow through the shared sprite descriptor.
 *
 * Same state machine and curves as menu_update_and_draw_sliding_cursor_variant; the
 * curve offset moves the quad's y edges instead of its x origin.
 */
void menu_update_and_draw_bouncing_cursor_variant(point16_t* pos, u16* state, s32 mode) {
    u16 unused[4]; /* never accessed; the sibling's rect keeps the 0x38-byte frame */
    s32 bottom;
    s32 active;
    s32 active_bit;
    s32 timer;
    s32 y_offset;
    s8 raw_offset;
    s32 i;
    u16 front_clut;
    u16 shadow_clut;

    active_bit = *state >> 15;
    active = active_bit;
    if (mode == 0) {
        if (active_bit != 0) {
            *state = 0;
            active = 0;
        }
    }
    active_bit = active;
    if (active_bit == 0 && (mode & 1) != 0) {
        *state = 0x8000;
        active = 1;
    }

    timer = *state;
    timer &= 0x7F;
    timer = timer * menu_get_event_speed();
    active_bit = active;
    if (active_bit != 0) {
        if (timer >= 0x25) {
            timer = 0x24;
        }
    } else {
        if (timer >= 0x2F) {
            timer = 0;
        }
    }

    active_bit = active;
    if (active_bit != 0) {
        for (i = 0; g_menu_cursor_reverse_offset_keyframes[i] < timer; i += 2) { }
        raw_offset = g_menu_cursor_reverse_offset_keyframes[i + 1];
    } else {
        for (i = 0; g_menu_cursor_forward_offset_keyframes[i] < timer; i += 2) { }
        raw_offset = g_menu_cursor_forward_offset_keyframes[i + 1];
    }

    y_offset = raw_offset;
    timer += menu_get_event_speed();
    active_bit = active;
    if (active_bit != 0) {
        if (timer < 0x25) {
            (*state)++;
        }
    } else {
        if (timer < 0x2F) {
            (*state)++;
        } else {
            *state = 0;
        }
    }

    if (mode != 0) {
        front_clut = g_menu_cursor_mode1_foreground_clut;
        shadow_clut = g_menu_cursor_mode1_background_clut;
    } else {
        front_clut = g_menu_cursor_mode0_foreground_clut;
        shadow_clut = g_menu_cursor_mode0_background_clut;
    }

    bottom = y_offset + 0x10;
    setXY4(&g_bunit_bouncing_cursor_quad, pos->x, y_offset + pos->y, pos->x + 0x10, y_offset + pos->y, pos->x,
        pos->y + bottom, pos->x + 0x10, pos->y + bottom);
    g_bunit_bouncing_cursor_quad.clut = front_clut;
    g_bunit_bouncing_cursor_quad.tpage = g_menu_cursor_texture_page;
    gfx_enqueue_textured_quad_corners(&g_bunit_bouncing_cursor_quad, 0, 0, 0x3C);
    setXY4(&g_bunit_bouncing_cursor_quad, pos->x, y_offset + pos->y + 2, pos->x + 0x10, y_offset + pos->y + 2, pos->x,
        pos->y + bottom + 2, pos->x + 0x10, pos->y + bottom + 2);
    g_bunit_bouncing_cursor_quad.clut = shadow_clut;
    g_bunit_bouncing_cursor_quad.tpage = 0x5F;
    gfx_enqueue_textured_quad_corners(&g_bunit_bouncing_cursor_quad, 0, 1, 0x3B);
}
