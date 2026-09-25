#include "fft/event_jobstts.h"

/* Two animation curves, each a stride-2 run of (frame threshold, horizontal
 * pixel offset) bytes: 0x801f0018 has 6 steps used while the marker is idle,
 * 0x801f0024 has 10 steps used while it is active. The target indexes both
 * with a single scaled byte offset, so each curve is read as a byte array at
 * [i] and [i + 1]; one struct array keeps an extra unscaled index live and
 * adds an instruction to each scan. The curve names are provisional. */

/* Advances the animated 16x16 marker at `anchor` and enqueues it with its
 * semi-transparent shadow.
 *
 * `state` packs an active flag in bit 15 and a frame counter in bits 0-6.
 * `mode` is the window variant: 0 forces the marker back to the idle curve,
 * an odd value latches the active curve. The counter is scaled by the menu
 * speed multiplier before the curve lookup and clamped to the end of the
 * curve in use.
 *
 * `active_test` reproduces the target's copy-then-branch at every read of
 * `active`, including the copy the first `if` leaves dead. Branching on
 * `active` directly, or dropping that dead copy, coalesces the two pseudos
 * and changes both the register assignment and the clamp join. */
void jobstts_menu_update_and_draw_animated_marker(urect16_t* anchor, u16* state, s32 mode) {
    urect16_t rect;
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
    frame = frame * jobstts_menu_get_event_speed();
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
        for (i = 0; g_jobstts_menu_cursor_reverse_offset_keyframes[i] < frame; i += 2) { }
        raw_offset = g_jobstts_menu_cursor_reverse_offset_keyframes[i + 1];
    } else {
        for (i = 0; g_jobstts_menu_cursor_forward_offset_keyframes[i] < frame; i += 2) { }
        raw_offset = g_jobstts_menu_cursor_forward_offset_keyframes[i + 1];
    }
    x_offset = (s8)raw_offset;

    frame += jobstts_menu_get_event_speed();
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
        clut = g_jobstts_menu_cursor_mode1_foreground_clut;
        shadow_clut = g_jobstts_menu_cursor_mode1_background_clut;
    } else {
        clut = g_jobstts_menu_cursor_mode0_foreground_clut;
        shadow_clut = g_jobstts_menu_cursor_mode0_background_clut;
    }

    rect.x = x_offset + anchor->x - 2;
    rect.y = anchor->y;
    rect.w = 0x10;
    rect.h = 0x10;
    jobstts_gfx_enqueue_textured_quad(
        &rect, 0xA8, 0, 0, 0, g_jobstts_menu_cursor_texture_page, clut, g_jobstts_gfx_otag_index);
    rect.x = rect.x + 2;
    rect.y = rect.y + 2;
    jobstts_gfx_enqueue_textured_quad(&rect, 0xB8, 0, 0, 1, 0x5F, shadow_clut, g_jobstts_gfx_otag_index);
}
