#include "fft/event_equip.h"
#include "psx/types.h"

/* Idle (0x801d87a0, 6 steps) and active (0x801d87ac, 10 steps) animation
 * curves of (frame threshold, horizontal pixel offset) byte pairs, declared as
 * separate stride-2 arrays to match the target's single scaled index. */

/* Texture page and the two CLUT pairs selected by `mode`. */

/* Advances the animated 16x16 marker at `anchor` and enqueues it with its
 * semi-transparent shadow.
 *
 * Twin of jobstts_menu_update_and_draw_animated_marker, which documents the
 * `state` packing and the `active_test` copies required for the match.
 */
void equip_menu_update_and_draw_animated_marker(const point16_t* anchor, u16* state, s32 mode) {
    RECT rect;
    s32 active;
    s32 active_test;
    s32 frame;
    s32 i;
    u8 raw_offset;
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
    frame = frame * equip_menu_get_event_speed();
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
        for (i = 0; g_equip_menu_cursor_reverse_offset_keyframes[i] < frame; i += 2) { }
        raw_offset = g_equip_menu_cursor_reverse_offset_keyframes[i + 1];
    } else {
        for (i = 0; g_equip_menu_cursor_forward_offset_keyframes[i] < frame; i += 2) { }
        raw_offset = g_equip_menu_cursor_forward_offset_keyframes[i + 1];
    }
    x_offset = (s8)raw_offset;

    frame += equip_menu_get_event_speed();
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
        clut = g_equip_menu_cursor_mode1_foreground_clut;
        shadow_clut = g_equip_menu_cursor_mode1_background_clut;
    } else {
        clut = g_equip_menu_cursor_mode0_foreground_clut;
        shadow_clut = g_equip_menu_cursor_mode0_background_clut;
    }

    rect.x = x_offset + anchor->x - 2;
    rect.y = anchor->y;
    rect.w = 0x10;
    rect.h = 0x10;
    equip_gfx_enqueue_textured_quad(
        &rect, 0xA8, 0, 0, 0, g_equip_menu_cursor_texture_page, clut, g_equip_gfx_sprite_ot_index);
    rect.x = rect.x + 2;
    rect.y = rect.y + 2;
    equip_gfx_enqueue_textured_quad(&rect, 0xB8, 0, 0, 1, 0x5F, shadow_clut, g_equip_gfx_sprite_ot_index);
}
