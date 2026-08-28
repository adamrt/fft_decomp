#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/*
 * Draw and advance the two-layer cursor used by equipment and item menus.
 *
 * Bit 15 selects the alternate animation. The `state_mode` copies are the
 * target's own `move v0,s2` before each test.
 */
void world_menu_draw_animated_cursor(world_menu_point_t* position, u16* state, s32 mode) {
    RECT rect;
    /* Pin required: unpinned, the roles held in $s2/$s3/$s4 rotate. */
    register s32 active __asm__("$20");
    u32 state_mode;
    s32 alternate;
    s32 frame;
    s32 index;
    u32 x_offset_word;
    s32 x_offset;
    u16 front_clut;
    u16 back_clut;

    active = mode;
    state_mode = *state >> 15;
    alternate = state_mode;
    if (active == 0) {
        state_mode = alternate;
        if (state_mode != 0) {
            *state = 0;
            /* Hides that active is 0 here, so alternate = 0 stays `move s2,zero`
             * instead of reusing $s4. */
            __asm__("" : "=r"(active) : "0"(active));
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
        x_offset_word = g_world_menu_cursor_bob_active_thresholds[index + 1] << 24;
    } else {
        index = 0;
        if ((s8)g_world_menu_cursor_bob_thresholds[0] < frame) {
            do {
                index += 2;
            } while ((s8)g_world_menu_cursor_bob_thresholds[index] < frame);
        }
        x_offset_word = g_world_menu_cursor_bob_thresholds[index + 1] << 24;
    }
    x_offset = (s32)x_offset_word >> 24;
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
    rect.x = x_offset + position->x - 2;
    rect.y = position->y;
    rect.w = 0x10;
    rect.h = 0x10;
    world_gfx_enqueue_textured_quad(
        &rect, 0xA8, 0, 0, 0, g_world_menu_icon_tpage, front_clut, g_world_menu_draw_priority);
    rect.x += 2;
    rect.y += 2;
    world_gfx_enqueue_textured_quad(&rect, 0xB8, 0, 0, 1, 0x5F, back_clut, g_world_menu_draw_priority);
}
