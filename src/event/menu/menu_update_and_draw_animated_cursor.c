#include "fft/menu.h"

/* Draw the sliding 16x16 marker and its semi-transparent shadow. State packs
 * the active flag in bit 15 and the frame counter in bits 0-6. Mode 0 cancels
 * the active curve; an odd mode starts it. The copy before each active test
 * preserves the original register allocation. */
void menu_update_and_draw_animated_cursor(const point16_t* anchor, u16* state, s32 mode) {
    RECT rect;
    s32 active;
    s32 active_test;
    s32 frame;
    s32 i;
    s8 raw_offset;
    s32 x_offset;
    u16 clut;
    u16 shadow_clut;

    active = *state >> 15;
    active_test = active;
    if (mode == 0) {
        if (active != 0) {
            *state = 0;
            active = 0;
        }
    }
    active_test = active;
    if (active_test == 0 && (mode & 1)) {
        *state = 0x8000;
        active = 1;
    }

    frame = *state;
    frame = frame & 0x7F;
    frame = frame * menu_get_event_speed();
    active_test = active;
    if (active_test != 0) {
        if (frame >= 0x25) {
            frame = 0x24;
        }
    } else if (frame >= 0x2F) {
        frame = 0;
    }

    active_test = active;
    if (active_test != 0) {
        for (i = 0; g_menu_cursor_reverse_offset_keyframes[i] < frame; i += 2) { }
        raw_offset = g_menu_cursor_reverse_offset_keyframes[i + 1];
    } else {
        for (i = 0; g_menu_cursor_forward_offset_keyframes[i] < frame; i += 2) { }
        raw_offset = g_menu_cursor_forward_offset_keyframes[i + 1];
    }
    x_offset = raw_offset;

    frame += menu_get_event_speed();
    active_test = active;
    if (active_test != 0) {
        if (frame < 0x25) {
            *state = *state + 1;
        }
    } else if (frame < 0x2F) {
        *state = *state + 1;
    } else {
        *state = 0;
    }

    if (mode != 0) {
        clut = g_menu_cursor_mode1_foreground_clut;
        shadow_clut = g_menu_cursor_mode1_background_clut;
    } else {
        clut = g_menu_cursor_mode0_foreground_clut;
        shadow_clut = g_menu_cursor_mode0_background_clut;
    }

    rect.x = x_offset + anchor->x - 2;
    rect.y = anchor->y;
    rect.w = 0x10;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(&rect, 0xA8, 0, 0, 0, g_menu_cursor_texture_page, clut, g_menu_gfx_otag_index);
    rect.x = rect.x + 2;
    rect.y = rect.y + 2;
    menu_gfx_enqueue_textured_quad(&rect, 0xB8, 0, 0, 1, 0x5F, shadow_clut, g_menu_gfx_otag_index);
}
