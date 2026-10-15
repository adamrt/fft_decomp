#include "fft/world.h"

/*
 * Draw and advance the vertical two-layer cursor used by menu prompts.
 *
 * This variant offsets a shared textured quad vertically before submitting
 * two `POLY_FT4` layers. Bit 15 selects the alternate animation.
 */
void world_menu_draw_animated_vertical_cursor(point16_t* position, u16* state, s32 mode) {
    RECT rect; /* never used; retail reserves its 8 bytes */
    u32 state_mode;
    s32 alternate;
    s32 frame;
    s32 index;
    s8 bob;
    s32 y_offset;
    s32 y_end;
    u16 front_clut;
    u16 back_clut;

    state_mode = *state >> 15;
    alternate = state_mode;
    if (mode == 0) {
        state_mode = alternate;
        if (state_mode != 0) {
            *state = 0;
            alternate = 0;
        }
    }
    state_mode = alternate;
    if (state_mode == 0 && (mode & 1)) {
        *state = 0x8000;
        alternate = 1;
    }
    frame = *state;
    frame &= 0x7F;
    frame *= menu_get_event_speed();
    state_mode = alternate;
    if (state_mode != 0) {
        if (frame >= 0x25) {
            frame = 0x24;
        }
    } else if (frame >= 0x2F) {
        frame = 0;
    }
    state_mode = alternate;
    if (state_mode != 0) {
        for (index = 0; g_world_menu_cursor_bob_active_thresholds[index] < frame; index += 2) { }
        bob = g_world_menu_cursor_bob_active_thresholds[index + 1];
    } else {
        for (index = 0; g_world_menu_cursor_bob_thresholds[index] < frame; index += 2) { }
        bob = g_world_menu_cursor_bob_thresholds[index + 1];
    }
    y_offset = bob;
    frame += menu_get_event_speed();
    state_mode = alternate;
    if (state_mode != 0) {
        if (frame < 0x25) {
            *state += 1;
        }
    } else if (frame < 0x2F) {
        *state += 1;
    } else {
        *state = 0;
    }
    if (mode != 0) {
        front_clut = g_world_menu_clut_front_alt;
        back_clut = g_world_menu_clut_back_alt;
    } else {
        front_clut = g_world_menu_clut_front;
        back_clut = g_world_menu_clut_back;
    }

    g_world_vertical_cursor_quad.x0 = position->x;
    g_world_vertical_cursor_quad.y0 = y_offset + position->y;
    g_world_vertical_cursor_quad.x1 = position->x + 0x10;
    g_world_vertical_cursor_quad.y1 = y_offset + position->y;
    g_world_vertical_cursor_quad.x2 = position->x;
    y_end = y_offset + 0x10;
    g_world_vertical_cursor_quad.y2 = position->y + y_end;
    g_world_vertical_cursor_quad.x3 = position->x + 0x10;
    g_world_vertical_cursor_quad.y3 = position->y + y_end;
    g_world_vertical_cursor_quad.clut = front_clut;
    g_world_vertical_cursor_quad.tpage = g_menu_cursor_texture_page;
    gfx_enqueue_textured_quad_corners(&g_world_vertical_cursor_quad, 0, 0, g_menu_gfx_otag_index);

    g_world_vertical_cursor_quad.x0 = position->x;
    g_world_vertical_cursor_quad.y0 = y_offset + position->y + 2;
    g_world_vertical_cursor_quad.x1 = position->x + 0x10;
    g_world_vertical_cursor_quad.y1 = y_offset + position->y + 2;
    g_world_vertical_cursor_quad.x2 = position->x;
    g_world_vertical_cursor_quad.y2 = position->y + y_end + 2;
    g_world_vertical_cursor_quad.x3 = position->x + 0x10;
    g_world_vertical_cursor_quad.y3 = position->y + y_end + 2;
    g_world_vertical_cursor_quad.clut = back_clut;
    g_world_vertical_cursor_quad.tpage = 0x5F;
    gfx_enqueue_textured_quad_corners(&g_world_vertical_cursor_quad, 0, 1, g_menu_gfx_otag_index);
}
