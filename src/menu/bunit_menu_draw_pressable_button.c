#include "fft/event_bunit.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Draw one slot's span of g_bunit_slot_sprite_entries sprite entries at (x, y).
 *
 * A nonzero `busy` clears the slot's g_bunit_menu_button_press_timers phase (CLUT
 * g_menu_cursor_mode1_foreground_clut); a nonzero `pressed` starts it and
 * stores `sound_id` to g_bunit_sound_queued_effect_id; otherwise an active
 * phase advances until 6 / event speed frames have passed (CLUT
 * g_menu_cursor_mode0_foreground_clut once idle). While a phase is
 * active the sprite is drawn one pixel lower with CLUT g_bunit_menu_button_clut. abs()
 * matters: GCC expands it to the single-insn abssi2, which keeps the entry
 * loop body one basic block as in the target. */
void bunit_menu_draw_pressable_button(s16 slot, s16 x, s16 y, s16 pressed, s16 busy, u16 sound_id) {
    oriented_quad_t desc;
    /* Eight untouched bytes plus descriptor alignment retain the target frame. */
    s16 unused_tail[4];
    u16 clut;
    s32 speed;
    s32 phase;
    s16 entry;
    s32 count;
    s32 i;
    s32 scale_x;
    s32 scale_y;
    u32 orientation;

    speed = menu_get_event_speed();
    if (busy != 0) {
        g_menu_button_press_timers[slot] = 0;
        clut = g_menu_cursor_mode1_foreground_clut;
    } else if (pressed != 0) {
        g_menu_button_press_timers[slot] = 1;
        g_sound_queued_effect_id = sound_id;
    } else {
        phase = g_menu_button_press_timers[slot];
        if ((phase != 0) && (phase < 6 / speed)) {
            g_menu_button_press_timers[slot] = g_menu_button_press_timers[slot] + 1;
        } else {
            clut = g_menu_cursor_mode0_foreground_clut;
            g_menu_button_press_timers[slot] = 0;
        }
    }

    i = 0;
    if (g_menu_button_press_timers[slot] != 0) {
        y += 1;
        clut = g_menu_button_clut;
    }
    entry = g_menu_button_piece_starts[slot];
    count = g_menu_button_piece_starts[slot + 1] - entry;
    desc.source.clut = clut;
    desc.source.tpage = g_menu_cursor_texture_page;
    if (count > 0) {
        do {
            menu_button_piece_t* sprite = &g_bunit_slot_sprite_entries[entry];
            desc.position.x = x + sprite->x_offset;
            desc.position.y = y + sprite->y_offset;
            desc.w = abs(g_bunit_menu_button_piece_scales[entry][0] * sprite->w) >> 12;
            desc.h = abs(g_bunit_menu_button_piece_scales[entry][1] * sprite->h) >> 12;
            desc.source.u = sprite->u;
            desc.source.v = sprite->v;
            desc.source.w = sprite->w;
            desc.source.h = sprite->h;
            scale_x = g_bunit_menu_button_piece_scales[entry][0];
            scale_y = g_bunit_menu_button_piece_scales[entry][1];
            orientation = scale_x < 0;
            if (scale_y < 0) {
                orientation += 2;
            }
            menu_gfx_enqueue_oriented_textured_quad(&desc, 0, orientation, 0, i + 0x32);
            i += 1;
            entry += 1;
        } while (i < count);
    }
}
