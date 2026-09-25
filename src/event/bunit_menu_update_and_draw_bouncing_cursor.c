#include "fft/event_bunit.h"
#include "psx/types.h"

/* Advances the animated cursor at `pos` and draws it, bouncing vertically,
 * with its semi-transparent shadow through the shared sprite descriptor.
 *
 * Same state machine and curves as bunit_menu_update_and_draw_animated_cursor; the
 * curve offset moves the quad's y edges instead of its x origin.
 */
void bunit_menu_update_and_draw_bouncing_cursor(u16* pos, u16* state, s32 mode) {
    u16 unused[4]; /* never accessed; the sibling's rect keeps the 0x38-byte frame */
    s32 bottom;
    s32 active;
    s32 active_bit;
    s32 timer;
    s32 offset;
    s32 offset_shifted;
    s32 i;
    u16 cursor_clut;
    u16 shadow_clut;

    active_bit = state[0] >> 15;
    active = active_bit;
    if (mode == 0) {
        if (active_bit != 0) {
            state[0] = 0;
            active = 0;
        }
    }
    active_bit = active;
    if (active_bit == 0 && (mode & 1) != 0) {
        state[0] = 0x8000;
        active = 1;
    }

    timer = state[0];
    timer &= 0x7F;
    timer = timer * bunit_menu_get_event_speed();
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
        for (i = 0; g_bunit_menu_cursor_reverse_offset_keyframes[i] < timer; i += 2) { }
        offset_shifted = g_bunit_menu_cursor_reverse_offset_bytes[i + 1] << 24;
    } else {
        for (i = 0; g_bunit_menu_cursor_forward_offset_keyframes[i] < timer; i += 2) { }
        offset_shifted = g_bunit_menu_cursor_forward_offset_bytes[i + 1] << 24;
    }

    offset = offset_shifted >> 24;
    timer += bunit_menu_get_event_speed();
    active_bit = active;
    if (active_bit != 0) {
        if (timer < 0x25) {
            state[0]++;
        }
    } else {
        if (timer < 0x2F) {
            state[0]++;
        } else {
            state[0] = 0;
        }
    }

    if (mode != 0) {
        cursor_clut = g_bunit_menu_cursor_mode1_foreground_clut;
        shadow_clut = g_bunit_menu_cursor_mode1_background_clut;
    } else {
        cursor_clut = g_bunit_menu_cursor_mode0_foreground_clut;
        shadow_clut = g_bunit_menu_cursor_mode0_background_clut;
    }

    bottom = offset + 0x10;
    g_bunit_bouncing_cursor_quad.x0 = pos[0];
    g_bunit_bouncing_cursor_quad.y0 = offset + pos[1];
    g_bunit_bouncing_cursor_quad.x1 = pos[0] + 0x10;
    g_bunit_bouncing_cursor_quad.y1 = offset + pos[1];
    g_bunit_bouncing_cursor_quad.x2 = pos[0];
    g_bunit_bouncing_cursor_quad.y2 = pos[1] + bottom;
    g_bunit_bouncing_cursor_quad.x3 = pos[0] + 0x10;
    g_bunit_bouncing_cursor_quad.y3 = pos[1] + bottom;
    g_bunit_bouncing_cursor_quad.clut = cursor_clut;
    g_bunit_bouncing_cursor_quad.tpage = g_bunit_menu_icon_texture_page;
    bunit_gfx_enqueue_textured_quad_corners(&g_bunit_bouncing_cursor_quad, 0, 0, 0x3C);
    g_bunit_bouncing_cursor_quad.x0 = pos[0];
    g_bunit_bouncing_cursor_quad.y0 = offset + pos[1] + 2;
    g_bunit_bouncing_cursor_quad.x1 = pos[0] + 0x10;
    g_bunit_bouncing_cursor_quad.y1 = offset + pos[1] + 2;
    g_bunit_bouncing_cursor_quad.x2 = pos[0];
    g_bunit_bouncing_cursor_quad.y2 = pos[1] + bottom + 2;
    g_bunit_bouncing_cursor_quad.x3 = pos[0] + 0x10;
    g_bunit_bouncing_cursor_quad.y3 = pos[1] + bottom + 2;
    g_bunit_bouncing_cursor_quad.clut = shadow_clut;
    g_bunit_bouncing_cursor_quad.tpage = 0x5F;
    bunit_gfx_enqueue_textured_quad_corners(&g_bunit_bouncing_cursor_quad, 0, 1, 0x3B);
}
