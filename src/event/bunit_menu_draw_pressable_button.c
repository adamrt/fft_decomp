#include "fft/bunit.h"
#include "psx/libc.h"
#include "psx/types.h"

/* 12-byte sprite entry: texture uv, source size, offset from the anchor. */
typedef struct {
    u16 u;
    u16 v;
    u16 w;
    u16 h;
    u16 dx;
    u16 dy;
} bunit_menu_button_sprite_entry_t;

extern bunit_menu_button_sprite_entry_t g_bunit_slot_sprite_entries[];

/* Draw one slot's span of g_bunit_slot_sprite_entries sprite entries at (x, y).
 *
 * A nonzero `busy` clears the slot's g_bunit_menu_button_press_timers phase (CLUT
 * g_bunit_menu_cursor_mode1_foreground_clut); a nonzero `pressed` starts it and
 * stores `sound_id` to g_bunit_sound_queued_effect_id; otherwise an active
 * phase advances until 6 / event speed frames have passed (CLUT
 * g_bunit_menu_cursor_mode0_foreground_clut once idle). While a phase is
 * active the sprite is drawn one pixel lower with CLUT g_bunit_menu_button_clut. abs()
 * matters: GCC expands it to the single-insn abssi2, which keeps the entry
 * loop body one basic block as in the target. */
void bunit_menu_draw_pressable_button(s16 slot, s16 x, s32 y, s16 pressed, s16 busy, u16 sound_id) {
    /* The target frame reserves 0x20 bytes for the descriptor; the tail is
     * never accessed. */
    struct {
        bunit_oriented_quad_t quad;
        s16 unused_14[6];
    } desc;
    u16 clut;
    s32 speed;
    s32 phase;
    s16 entry;
    s32 count;
    s32 i;
    s32 scale_x;
    s32 scale_y;
    u32 orientation;

    speed = bunit_menu_get_event_speed();
    if (busy != 0) {
        g_bunit_menu_button_press_timers[slot] = 0;
        clut = g_bunit_menu_cursor_mode1_foreground_clut;
    } else if (pressed != 0) {
        g_bunit_menu_button_press_timers[slot] = 1;
        g_bunit_sound_queued_effect_id = sound_id;
    } else {
        phase = g_bunit_menu_button_press_timers[slot];
        if ((phase != 0) && (phase < 6 / speed)) {
            g_bunit_menu_button_press_timers[slot] = g_bunit_menu_button_press_timers[slot] + 1;
        } else {
            clut = g_bunit_menu_cursor_mode0_foreground_clut;
            g_bunit_menu_button_press_timers[slot] = 0;
        }
    }

    i = 0;
    if (g_bunit_menu_button_press_timers[slot] != 0) {
        y += 1;
        clut = g_bunit_menu_button_clut;
    }
    entry = g_bunit_menu_button_piece_starts[slot];
    count = g_bunit_menu_button_piece_starts[slot + 1] - entry;
    desc.quad.clut = clut;
    desc.quad.tpage = g_bunit_menu_icon_texture_page;
    if (count > 0) {
        do {
            bunit_menu_button_sprite_entry_t* sprite = &g_bunit_slot_sprite_entries[entry];
            desc.quad.x = x + sprite->dx;
            desc.quad.y = y + sprite->dy;
            desc.quad.w = abs(g_bunit_menu_button_piece_scales[entry][0] * (s16)sprite->w) >> 12;
            desc.quad.h = abs(g_bunit_menu_button_piece_scales[entry][1] * (s16)sprite->h) >> 12;
            desc.quad.u = sprite->u;
            desc.quad.v = sprite->v;
            desc.quad.uw = sprite->w;
            desc.quad.vh = sprite->h;
            scale_x = g_bunit_menu_button_piece_scales[entry][0];
            scale_y = g_bunit_menu_button_piece_scales[entry][1];
            orientation = scale_x < 0;
            if (scale_y < 0) {
                orientation += 2;
            }
            bunit_gfx_enqueue_oriented_textured_quad(&desc.quad, 0, orientation, 0, i + 0x32);
            i += 1;
            entry += 1;
        } while (i < count);
    }
}
