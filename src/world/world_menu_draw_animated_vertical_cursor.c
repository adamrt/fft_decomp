#include "fft/world.h"

/*
 * Draw and advance the vertical two-layer cursor used by menu prompts.
 *
 * This variant offsets a shared textured quad vertically before submitting
 * two `POLY_FT4` layers. Bit 15 selects the alternate animation.
 */
void world_menu_draw_animated_vertical_cursor(world_menu_point_t* position, u16* state, s32 mode) {
    s32 active;
    u32 state_mode;
    s32 alternate;
    s32 frame;
    s32 index;
    u32 y_offset_word;
    s32 y_offset;
    s32 y_end;
    volatile s32 unused;
    u16 front_clut;
    u16 back_clut;

    active = mode;
    state_mode = *state >> 15;
    alternate = state_mode;
    if (active == 0) {
        state_mode = alternate;
        if (state_mode != 0) {
            *state = 0;
            alternate = 0;
        }
    }
    state_mode = alternate;
    if (state_mode == 0 && (active & 1)) {
        *state = 0x8000;
        alternate = 1;
    }
    frame = *state;
    frame &= 0x7F;
    frame *= world_gfx_get_vsync_mode_or_one();
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
        index = 0;
        if ((s8)g_world_menu_cursor_bob_active_thresholds[0] < frame) {
            do {
                index += 2;
            } while ((s8)g_world_menu_cursor_bob_active_thresholds[index] < frame);
        }
        y_offset_word = g_world_menu_cursor_bob_active_thresholds[index + 1] << 24;
    } else {
        index = 0;
        if ((s8)g_world_menu_cursor_bob_thresholds[0] < frame) {
            do {
                index += 2;
            } while ((s8)g_world_menu_cursor_bob_thresholds[index] < frame);
        }
        y_offset_word = g_world_menu_cursor_bob_thresholds[index + 1] << 24;
    }
    y_offset = (s32)y_offset_word >> 24;
    frame += world_gfx_get_vsync_mode_or_one();
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
    if (active != 0) {
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
    g_world_vertical_cursor_quad.tpage = g_world_menu_icon_tpage;
    world_gfx_append_poly_ft4_to_otag(&g_world_vertical_cursor_quad, 0, 0, g_world_menu_draw_priority);

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
    world_gfx_append_poly_ft4_to_otag(&g_world_vertical_cursor_quad, 0, 1, g_world_menu_draw_priority);
}
