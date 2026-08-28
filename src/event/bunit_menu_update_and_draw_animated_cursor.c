#include "fft/bunit.h"
#include "psx/types.h"

/* Advances the animated cursor at `pos` and draws it, sliding horizontally,
 * with its semi-transparent shadow. `mode` bit 0 starts the reverse curve;
 * mode 0 cancels it. */
void bunit_menu_update_and_draw_animated_cursor(u16* pos, u16* state, s32 mode) {
    RECT rect;
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

    rect.x = offset + pos[0] - 2;
    rect.y = pos[1];
    rect.w = 0x10;
    rect.h = 0x10;
    bunit_gfx_enqueue_textured_quad(&rect, 0xA8, 0, 0, 0, g_bunit_menu_icon_texture_page, cursor_clut, 0x3C);
    rect.x += 2;
    rect.y += 2;
    bunit_gfx_enqueue_textured_quad(&rect, 0xB8, 0, 0, 1, 0x5F, shadow_clut, 0x3B);
}
