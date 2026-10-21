/* Browse the Unit List’s battle-unit grid and draw its readouts, shaded
 * rows, trailing cursor and animated unit sprites from the same grid state. */

#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_list.h"
#include "fft/unit_panel.h"
#include "fft/unit_runtime.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Dimensions selected by the four-bit size code in an SHP frame part. */
typedef struct sprite_size {
    u8 width;
    u8 height;
} sprite_size_t;
typedef char sprite_size_must_be_2[sizeof(sprite_size_t) == 2 ? 1 : -1];

/* Byte-aligned display streams, including the terminating opcode. */
extern u8 g_unit_list_grid_stat_bar_script_top[12];
extern u8 g_unit_list_grid_stat_bar_script_bottom[12];

/* unit grid */
extern point16_t g_unit_list_grid_cursor_target;
extern u8 g_unit_list_stat_mode; /* menu_unit_readout_e */
static s16 s_unit_list_current_stat_value;
extern s16 g_unit_list_max_stat_value;
extern menu_sprite_cell_t g_unit_list_grid_label_tiles[8];
extern menu_sprite_cell_t g_unit_list_grid_label_tiles_bottom[8];
extern menu_draw_number_command_t g_unit_list_grid_level_number;
extern menu_draw_number_command_t g_unit_list_grid_experience_number;
extern menu_draw_number_command_t g_unit_list_grid_brave_number;
extern menu_draw_number_command_t g_unit_list_grid_brave_number_wide;
extern menu_draw_number_command_t g_unit_list_grid_faith_number;
extern menu_draw_number_command_t g_unit_list_grid_faith_number_wide;
extern menu_draw_number_command_t g_unit_list_grid_stat_number;
extern menu_draw_number_command_t g_unit_list_grid_max_stat_number;
extern s8 g_unit_list_grid_gauge_offsets[];
extern u16 g_unit_list_grid_hp_gauge_clut;
extern rgb24_t g_unit_list_grid_hp_gauge_colors[4];
extern u16 g_unit_list_grid_mp_gauge_clut;
extern rgb24_t g_unit_list_grid_mp_gauge_colors[4];
extern u16 g_unit_list_grid_ct_gauge_clut;
extern rgb24_t g_unit_list_grid_ct_gauge_colors[4];
extern rgb24_t g_unit_list_grid_normal_color;
extern rgb24_t g_unit_list_grid_dimmed_color;
extern oriented_quad_t g_unit_list_dot_sprite;
extern oriented_quad_t g_unit_list_shadow_sprite;
extern s8 g_unit_list_dot_glow_direction;
extern s8 g_unit_list_dot_glow_timer;
extern menu_button_piece_t g_unit_list_slot_sprite_entries[];

/* SHP frame-part size codes, in their encoded order. Kept mutable because
 * const changes the original load/store scheduling in the sprite builder. */
static sprite_size_t s_unit_list_sprite_cell_sizes[16]
    = { { 8, 8 }, { 16, 8 }, { 16, 16 }, { 16, 24 }, { 24, 8 }, { 24, 16 }, { 24, 24 }, { 32, 8 }, { 32, 16 },
          { 32, 24 }, { 32, 32 }, { 32, 40 }, { 40, 16 }, { 40, 32 }, { 48, 48 }, { 56, 56 } };

/* Signed 4.12 x/y scale pair per sprite entry; a negative scale flips the quad. */
extern point16_t g_unit_list_button_piece_scales[];

/* cursor trail */
/* Oriented-quad descriptors for the two cursor halves. */
extern oriented_quad_t g_unit_list_cursor_sprite;
extern oriented_quad_t g_unit_list_cursor_shadow_sprite;

/* Rhombus-cursor trail, oldest point first; x == -1 marks an empty slot
 * (cursor_trail_init clears all eight). */
extern point16_t g_unit_list_cursor_trail_points[8];
extern u8 g_unit_list_cursor_trail_brightness[];

/* row shading */
extern u16 g_unit_list_gfx_background_clut;
extern u16 g_unit_list_gfx_background_tile_layout[2][8]; /* per row parity: four (x, u) column pairs */
extern s16 g_unit_list_gfx_falloff_focus_x;
extern s16 g_unit_list_gfx_falloff_focus_y;

static void unit_sprite_draw(s16 unit, s32 x, s32 y);
static void cursor_trail_init(void);
static void cursor_trail_update(point16_t* target);

/* With `enable`, start the equipment/ability panel and numeric panel threads unless
 * the status panel is sliding or the panel thread already runs; without it, ask both
 * threads to stop. */
void unit_panel_toggle_equipment_and_numbers(s32 enable) {
    if (enable != 0) {
        if (g_unit_panel_status_display_thread_params.flags
            & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP)) {
            return;
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) != 0) {
            return;
        }
        thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, unit_panel_equipment_ability_thread);
        thread_parameters_set(THREAD_SCREEN_SLOT_CHARACTER_STATUS, &g_unit_panel_character_status_frame_config, 0, 0);
        thread_start(THREAD_SCREEN_SLOT_NUMERIC_PANEL, unit_panel_numbers_thread);
        thread_parameters_set(THREAD_SCREEN_SLOT_NUMERIC_PANEL, &g_unit_panel_numbers_thread_params, 0, 0);
    } else {
        menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
    }
}

/* Return the listed unit's current HP, MP, or CT value, as
 * g_unit_list_stat_mode selects. For any other mode the value returned is
 * never assigned. */
static s32 grid_stat_get(s32 unit_index) {
    menu_unit_readout_e mode = g_unit_list_stat_mode;
    unit_list_record_t* entry;
    s32 value;

    if (mode == MENU_UNIT_READOUT_HP) {
        entry = g_unit_list_pointers[unit_index];
        value = entry->gauges[MENU_UNIT_STATUS_GAUGE_HP].value;
    } else if (mode == MENU_UNIT_READOUT_MP) {
        entry = g_unit_list_pointers[unit_index];
        value = entry->gauges[MENU_UNIT_STATUS_GAUGE_MP].value;
    } else if (mode == MENU_UNIT_READOUT_CT) {
        entry = g_unit_list_pointers[unit_index];
        value = entry->gauges[MENU_UNIT_STATUS_GAUGE_CT].value;
    }
    s_unit_list_current_stat_value = value;
    return s_unit_list_current_stat_value;
}

/* Return the level of the unit in this unit list slot. */
static s32 grid_level_get(s32 unit_id) {
    return g_unit_list_pointers[unit_id]->level;
}

/* Return the experience of the unit in this unit list slot. */
static s32 grid_experience_get(s32 unit_id) {
    return g_unit_list_pointers[unit_id]->experience;
}

/* Return the Brave of the unit in this unit list slot. */
static s32 grid_brave_get(s32 unit_id) {
    return g_unit_list_pointers[unit_id]->brave;
}

/* Return the Faith of the unit in this unit list slot. */
static s32 grid_faith_get(s32 unit_id) {
    return g_unit_list_pointers[unit_id]->faith;
}

/* Return the selected unit's maximum HP, MP, or CT value. */
static s32 grid_stat_max_get(s32 unit_index) {
    s32 value;

    if (g_unit_list_stat_mode == MENU_UNIT_READOUT_HP) {
        value = g_unit_list_pointers[unit_index]->gauges[MENU_UNIT_STATUS_GAUGE_HP].max;
    } else if (g_unit_list_stat_mode == MENU_UNIT_READOUT_MP) {
        value = g_unit_list_pointers[unit_index]->gauges[MENU_UNIT_STATUS_GAUGE_MP].max;
    } else if (g_unit_list_stat_mode == MENU_UNIT_READOUT_CT) {
        value = UNIT_TURN_CT_THRESHOLD;
    }
    g_unit_list_max_stat_value = value;
    return g_unit_list_max_stat_value;
}

/* Returns 1 when the selected unit is in the two upper visible rows of the list at
 * `page_offset` (after the scroll in progress), which puts the status banner at the bottom
 * of the screen. Returns 0 while browsing units. */
static s32 upper_row_selection_check(s32 page_offset) {
    s32 in_upper_rows;
    s8 offset = page_offset;
    s32 scroll_step;

    if (g_unit_list_browse_enabled == 0) {
        in_upper_rows = 1;
        scroll_step = g_unit_list_scroll_step;
        if (scroll_step < 0) {
            offset = page_offset - 0x3c;
        }
        if (scroll_step > 0) {
            offset += 0x3c;
        }
        switch (offset) {
        case -0x78:
            if (g_unit_selected_index >= 0x10) {
                in_upper_rows = 0;
            }
            break;
        case -0x3c:
            if (g_unit_selected_index >= 0xc) {
                in_upper_rows = 0;
            }
            break;
        case 0:
            if (g_unit_selected_index >= 8) {
                in_upper_rows = 0;
            }
            break;
        }
    } else {
        in_upper_rows = 0;
    }
    return in_upper_rows;
}

/* Animate the unit list page offset: when idle, start a +4/-4 per-frame step
 * if the selected unit lies outside the page shown at the current offset
 * (0, -0x3C, -0x78 = pages of four units); once a step reaches +/-0x3C,
 * commit it to the offset and stop. */
static void grid_scroll_update(s8* step, s8* offset) {
    s8 cur = *step;
    s8 next;

    if (cur == 0) {
        switch (*offset) {
        case -0x78:
            if (g_unit_selected_index < 8) {
                *step = 4;
            }
            break;
        case -0x3C:
            if (g_unit_selected_index < 0x10) {
                if (g_unit_selected_index < 4) {
                    *step = 4;
                }
            } else {
                *step = -4;
            }
            break;
        case 0:
            if (g_unit_selected_index >= 0xC) {
                *step = -4;
            }
            break;
        }
    } else if (cur < 0) {
        next = cur - 4;
        *step = next;
        if (next < -0x3B) {
            *step = 0;
            *offset -= 0x3C;
        }
    } else if (cur > 0) {
        next = cur + 4;
        *step = next;
        if (next >= 0x3C) {
            *step = 0;
            *offset += 0x3C;
        }
    }
}

/* Draw one slot's span of g_unit_list_slot_sprite_entries sprite entries at (x, y).
 *
 * A nonzero `busy` clears the slot's g_menu_button_press_timers phase (CLUT
 * g_menu_cursor_mode1_foreground_clut); a nonzero `pressed` starts it and
 * stores `sound_id` to g_sound_queued_effect_id; otherwise an active
 * phase advances until 6 / event speed frames have passed (CLUT
 * g_menu_cursor_mode0_foreground_clut once idle). While a phase is
 * active the sprite is drawn one pixel lower with CLUT g_menu_button_clut. abs()
 * matters: GCC expands it to the single-insn abssi2, which keeps the entry
 * loop body one basic block as in the target. */
static void button_draw(s16 slot, s16 x, s16 y, s16 pressed, s16 busy, u16 sound_id) {
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
            menu_button_piece_t* sprite = &g_unit_list_slot_sprite_entries[entry];
            desc.position.x = x + sprite->x_offset;
            desc.position.y = y + sprite->y_offset;
            desc.w = abs(g_unit_list_button_piece_scales[entry].x * sprite->w) >> 12;
            desc.h = abs(g_unit_list_button_piece_scales[entry].y * sprite->h) >> 12;
            desc.source.u = sprite->u;
            desc.source.v = sprite->v;
            desc.source.w = sprite->w;
            desc.source.h = sprite->h;
            scale_x = g_unit_list_button_piece_scales[entry].x;
            scale_y = g_unit_list_button_piece_scales[entry].y;
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

/* Set up the unit list page: an empty scrollable list, the row callbacks for
 * current and maximum stats, level, experience, Brave and Faith, and the
 * status banner's position (at the bottom, y 0x90, while the selected unit
 * is in the upper rows). The argument is not read. */
void unit_list_init(s32 unused_selected_index) {
    s32 ret;

    menu_scrollable_list_core_init(0, 0, 0);
    g_menu_row_callbacks[0].value = grid_stat_get;
    g_menu_row_callbacks[1].value = grid_stat_max_get;
    g_menu_row_callbacks[4].value = grid_level_get;
    g_menu_row_callbacks[5].value = grid_experience_get;
    g_menu_row_callbacks[6].value = grid_brave_get;
    g_menu_row_callbacks[7].value = grid_faith_get;
    menu_set_vsync_mode_and_event_speed(0);
    ret = upper_row_selection_check(g_unit_list_page_offset);
    g_unit_list_status_banner_at_bottom = ret;
    g_unit_list_status_banner_at_bottom_latch = ret;
    if (g_unit_list_status_banner_at_bottom != 0) {
        g_unit_panel_status_display_thread_params.position.y = 0x90;
    } else {
        g_unit_panel_status_display_thread_params.position.y = 0;
    }
    cursor_trail_init();
}

/*
 * Formation-screen unit grid for the unit list menu: handle the stat-mode,
 * browse and banner toggles, step the grid cursor, draw each visible unit
 * panel with its stat readout for g_unit_list_stat_mode (layouts 0-2 label,
 * two numbers and gauge; 3 and 4 two labeled numbers), then ease the cursor
 * trail toward the selected cell.
 *
 * Twin of WORLD's formation_grid_update with unit_readout_draw's panel
 * body written inline. Details the target needs:
 * - `unit` is also the early "enabled" flag, `row` also the stat tile index
 *   and cursor delta, and `top` the button y, highlight flag and panel y, as
 *   in the target's shared registers and stack slots. The cell x is not a
 *   variable: loop.c strength-reduces `col * 62` into the last stack slot.
 * - The busy argument of button_draw is an inline `||` each time.
 * - number_y*, label_y* are single-set narrow copies of `top` assigned
 *   before a call; local-alloc sinks each to its use (the target's
 *   `move v0,s2` before the store). Their modes must differ from each other
 *   where two copy the same value.
 * - Single-statement bodies in the column loop are deliberately unbraced
 *   and the g_unit_list_browse_enabled and range tests are one condition: every braced
 *   block adds a note to the loop's LUID span, and one more makes loop.c
 *   hoist the shared constant 3 out of the row loop, where it also absorbs
 *   the layout-4 3x3 size that the target keeps local in s0.
 * - Layouts 0-2 read g_unit_list_grid_label_tiles as bytes: indexing the struct array changes
 *   the base constants (the target uses g_unit_list_grid_label_tiles+2 and +5). This raw
 *   offset is required for the byte-exact match.
 * - The two s16 cursor values give the frame's unreferenced 8-byte slots
 *   together with the two g_unit_selected_index tests; the unused
 *   RECTs reproduce the locals area.
 */
s32 unit_list_update_grid(s16 allow_banner_toggle, s16 allow_browse, s16 show_stat_panel, s16 allow_stat_cycle,
    s16 stat_mode, s16 buttons, s16 locked) {
    RECT unused[3];
    RECT cell;
    RECT rect;
    upoint16_t bar_points[4];
    RECT unused2;
    rgb24_t saved_color;
    s32 row;
    s32 col;
    u8* menu;
    menu_sprite_cell_t* quads;
    s16 y;
    s32 unit;
    rgb24_t* color;
    s32 unit_index;
    s32 cell_y;
    s32 top;
    s32 upper_y4;
    s32 upper_y3;
    u32 mode;
    u16 width;
    u16 clut;
    rgb24_t* gradient;
    s16 lower_y4;
    u8 number_y;
    u8 number_y3;
    s16 label_y3;
    s16 label_y4;
    s16 cursor_x;
    s16* focus;
    s16 cursor_y;

    if (locked != 0) {
        buttons = 0;
    }
    /* L2 and R2 cycle the stat readout: HP, MP, CT, level/experience, Brave/Faith. */
    if (allow_stat_cycle != 0) {
        if (g_unit_list_browse_enabled == 0) {
            if (buttons & PSX_PAD_L2) {
                g_unit_list_stat_mode = g_unit_list_stat_mode == MENU_UNIT_READOUT_HP ? MENU_UNIT_READOUT_BRAVE_FAITH
                                                                                      : g_unit_list_stat_mode - 1;
                buttons &= ~PSX_PAD_R2;
                g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
            }
            if (buttons & PSX_PAD_R2) {
                g_unit_list_stat_mode = g_unit_list_stat_mode == MENU_UNIT_READOUT_BRAVE_FAITH
                    ? MENU_UNIT_READOUT_HP
                    : g_unit_list_stat_mode + 1;
                g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
            }
        }
    }
    if (allow_stat_cycle == 0) {
        g_unit_list_stat_mode = stat_mode;
    }
    /* CIRCLE starts browsing units with their equipment panels and CROSS ends it; `unit`
     * drops to 0 on the frame browsing ends. */
    unit = 1;
    if (allow_browse != 0 && unit_list_get_input_lock() == 0
        && thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) == 0
        && !(g_unit_panel_status_display_thread_params.flags
            & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP))
        && g_unit_list_status_banner_slide_step == 0 && g_unit_list_scroll_step == 0) {
        if (buttons & PSX_PAD_CIRCLE) {
            if (g_unit_list_browse_enabled == 0 && thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0) {
                g_unit_list_browse_enabled = 1;
                g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            }
        } else if (buttons & PSX_PAD_CROSS) {
            if (g_unit_list_browse_enabled != 0) {
                unit = 0;
                g_sound_queued_effect_id = SOUND_SFX_CANCEL;
                g_unit_list_browse_enabled = 0;
            }
        }
    }
    if (g_unit_list_browse_enabled != 0) {
        buttons &= 0xFFF;
    }
    /* SQUARE shows or hides the status banner. */
    if (allow_banner_toggle != 0 && g_unit_list_browse_enabled == 0 && (buttons & PSX_PAD_SQUARE)
        && unit_list_is_scroll_needed() == 0 && thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0
        && thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) == 0
        && !(g_unit_panel_status_display_thread_params.flags
            & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP))) {
        g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
        g_unit_list_status_banner_enabled = g_unit_list_status_banner_enabled == 0;
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0 && g_unit_list_help_screen_id == MENU_GRID_HELP_NONE
        && gfx_get_fade_state() == 0) {
        g_unit_list_grid_selected_cell = menu_grid_selection_step(
            4, 5, g_unit_list_reorderable_count - 1, g_unit_list_grid_selected_cell, buttons);
        g_unit_selected_index = g_unit_list_reorderable_indices[g_unit_list_grid_selected_cell];
    }
    grid_scroll_update(&g_unit_list_scroll_step, &g_unit_list_page_offset);
    /* While browsing, L1 and R1 step through every listed unit, mounts included. */
    if (g_unit_list_browse_enabled != 0) {
        if (buttons & PSX_PAD_L1) {
            g_unit_selected_index
                = g_unit_selected_index == 0 ? g_unit_list_visible_count - 1 : g_unit_selected_index - 1;
        } else if (buttons & PSX_PAD_R1) {
            g_unit_selected_index
                = g_unit_selected_index != g_unit_list_visible_count - 1 ? g_unit_selected_index + 1 : 0;
        }
        g_unit_list_grid_selected_cell = g_unit_list_index_map[g_unit_selected_index];
        button_draw(MENU_PRESSABLE_BUTTON_L1, 0xC, 0xE, buttons & PSX_PAD_L1,
            unit_list_is_busy() != 0 || g_unit_list_help_screen_id != MENU_GRID_HELP_NONE, SOUND_SFX_PAGE_SWITCH);
        button_draw(MENU_PRESSABLE_BUTTON_R1, 0xE0, 0xE, buttons & PSX_PAD_R1,
            unit_list_is_busy() != 0 || g_unit_list_help_screen_id != MENU_GRID_HELP_NONE, SOUND_SFX_PAGE_SWITCH);
    }
    /* The stat-mode bar on the side away from the banner, with its L2 and R2 buttons and the
     * current mode's labels lit. */
    if (g_unit_list_status_banner_slide_step == 0 && unit_list_get_input_lock() == 0 && unit != 0
        && show_stat_panel != 0 && g_unit_list_status_banner_enabled != 0 && g_unit_list_browse_enabled == 0) {
        top = 0xF;
        if (g_unit_list_status_banner_at_bottom != 0) {
            menu = g_unit_list_grid_stat_bar_script_top;
            quads = g_unit_list_grid_label_tiles;
        } else {
            top = 0xDB;
            menu = g_unit_list_grid_stat_bar_script_bottom;
            quads = g_unit_list_grid_label_tiles_bottom;
        }
        button_draw(MENU_PRESSABLE_BUTTON_L2, 0x26, top, buttons & PSX_PAD_L2,
            unit_list_is_busy() != 0 || g_unit_list_help_screen_id != MENU_GRID_HELP_NONE, SOUND_SFX_PAGE_SWITCH);
        button_draw(MENU_PRESSABLE_BUTTON_R2, 0xC2, top, buttons & PSX_PAD_R2,
            unit_list_is_busy() != 0 || g_unit_list_help_screen_id != MENU_GRID_HELP_NONE, SOUND_SFX_PAGE_SWITCH);
        menu_run_script_with_palette_mode(menu, 0, 0);
        menu_gfx_enqueue_textured_quad_list(quads, 0, 0, g_menu_cursor_texture_page, g_menu_text_clut_2_mode0, 0xB, 8);
        mode = g_unit_list_stat_mode;
        if (mode < 3) {
            row = mode;
            col = 1;
        } else if (mode == MENU_UNIT_READOUT_LEVEL_EXP) {
            row = 3;
            col = 2;
        } else if (mode == MENU_UNIT_READOUT_BRAVE_FAITH) {
            row = 5;
            col = 3;
        }
        menu_gfx_enqueue_textured_quad_list(
            quads + row, 0, 0, g_menu_cursor_texture_page, g_menu_text_clut_1_mode0, 0xC, col);
    }
    /* The grid: up to five rows of four cells, 0x3c apart; rows under the banner are dimmed.
     * Each cell shows the stat readout, the unit's sprite and plate, and a rider's mount. */
    row = 0;
    g_unit_list_status_banner_at_bottom = upper_row_selection_check(g_unit_list_page_offset);
    y = 0x24 + g_unit_list_page_offset + g_unit_list_scroll_step;
    cell.w = 0x38;
    cell.h = 0x30;
    gfx_set_otag_index(8);
    unit = 0;
    color = menu_gfx_get_draw_color();
    saved_color.r = color->r;
    saved_color.g = color->g;
    saved_color.b = color->b;
    for (; row < 5; row++, y += 0x3C) {
        top = 0;
        if (g_unit_list_status_banner_enabled != 0) {
            if (g_unit_list_status_banner_at_bottom != 0) {
                top = y > 0x8F;
            } else if (y < 0x47) {
                top = 1;
            }
        }
        gfx_set_draw_color(top ? &g_unit_list_grid_dimmed_color : &g_unit_list_grid_normal_color);
        for (col = 0; col < 4; col++, unit++) {
            if (unit >= g_unit_list_reorderable_count)
                break;
            cell.x = col * 62 + 6;
            cell.y = y;
            if (unit == g_unit_list_grid_selected_cell) {
                g_unit_list_grid_cursor_target.x = col * 62 + 0x22;
                g_unit_list_grid_cursor_target.y = y + 0x19;
            }
            if (y < -0x3C || y >= 0xF0)
                continue;
            cell_y = cell.y;
            unit_index = g_unit_list_reorderable_indices[unit];
            top = cell_y + 40;
            if (g_unit_list_browse_enabled == 0 && top < 240) {
                if (g_unit_list_stat_mode < 3) {
                    number_y = top;
                    rect.x = cell.x;
                    rect.y = top;
                    rect.w = ((u8*)g_unit_list_grid_label_tiles)[g_unit_list_stat_mode * sizeof(menu_sprite_cell_t)
                        + offsetof(menu_sprite_cell_t, w)];
                    rect.h = ((u8*)g_unit_list_grid_label_tiles)[g_unit_list_stat_mode * sizeof(menu_sprite_cell_t)
                        + offsetof(menu_sprite_cell_t, h)];
                    menu_gfx_enqueue_textured_quad(&rect,
                        ((u8*)g_unit_list_grid_label_tiles)[g_unit_list_stat_mode * sizeof(menu_sprite_cell_t)
                            + offsetof(menu_sprite_cell_t, u)],
                        ((u8*)g_unit_list_grid_label_tiles)[g_unit_list_stat_mode * sizeof(menu_sprite_cell_t)
                            + offsetof(menu_sprite_cell_t, v)],
                        color, 0, g_menu_cursor_texture_page, g_menu_alternate_number_clut, 8);
                    g_unit_list_grid_max_stat_number.x = cell.x + 40;
                    g_unit_list_grid_max_stat_number.y = cell_y + 45;
                    g_unit_list_grid_max_stat_number.value = unit_index;
                    menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_unit_list_grid_max_stat_number);
                    g_unit_list_grid_stat_number.x = cell.x + 20;
                    g_unit_list_grid_stat_number.y = number_y;
                    g_unit_list_grid_stat_number.value = unit_index;
                    menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_unit_list_grid_stat_number);
                    rect.x = cell.x + 35;
                    rect.y = top + 5;
                    rect.w = 6;
                    rect.h = 11;
                    menu_gfx_enqueue_textured_quad(
                        &rect, 0xB4, 0x10, color, 0, g_menu_atlas_texture_page, g_menu_alternate_number_clut, 8);
                    if (g_unit_list_stat_mode == MENU_UNIT_READOUT_HP) {
                        clut = g_unit_list_grid_hp_gauge_clut;
                        gradient = g_unit_list_grid_hp_gauge_colors;
                    } else if (g_unit_list_stat_mode == MENU_UNIT_READOUT_MP) {
                        clut = g_unit_list_grid_mp_gauge_clut;
                        gradient = g_unit_list_grid_mp_gauge_colors;
                    } else {
                        clut = g_unit_list_grid_ct_gauge_clut;
                        gradient = g_unit_list_grid_ct_gauge_colors;
                    }
                    rect.x = cell.x;
                    rect.y = top + 8;
                    rect.w = 0x25;
                    rect.h = 3;
                    menu_gfx_enqueue_textured_quad(&rect, 0xD8, 0xCA, color, 0, g_menu_cursor_texture_page, clut, 7);
                    bar_points[0].x = g_unit_list_grid_gauge_offsets[0] + cell.x;
                    bar_points[0].y = g_unit_list_grid_gauge_offsets[1] + top + 8;
                    if (g_unit_list_max_stat_value != 0) {
                        width = (s_unit_list_current_stat_value << 5) / g_unit_list_max_stat_value;
                    } else {
                        width = 0;
                    }
                    bar_points[3].x = bar_points[1].x = width;
                    bar_points[1].x += bar_points[0].x;
                    bar_points[1].y = g_unit_list_grid_gauge_offsets[1] + top + 8;
                    bar_points[2].x = g_unit_list_grid_gauge_offsets[2] + cell.x;
                    bar_points[2].y = g_unit_list_grid_gauge_offsets[3] + top + 8;
                    bar_points[3].x += bar_points[2].x;
                    bar_points[3].y = g_unit_list_grid_gauge_offsets[3] + top + 8;
                    gfx_enqueue_gouraud_quad(bar_points, gradient, 0, 8);
                } else if (g_unit_list_stat_mode == MENU_UNIT_READOUT_LEVEL_EXP) {
                    top = cell_y + 45;
                    rect.x = cell.x - 1;
                    label_y3 = top;
                    number_y3 = top;
                    upper_y3 = cell_y + 40;
                    rect.y = upper_y3;
                    rect.w = g_unit_list_grid_label_tiles[3].w + 3;
                    rect.h = g_unit_list_grid_label_tiles[3].h + 1;
                    menu_gfx_enqueue_textured_quad(&rect, g_unit_list_grid_label_tiles[3].u,
                        g_unit_list_grid_label_tiles[3].v, color, 0, g_menu_cursor_texture_page,
                        g_menu_alternate_number_clut, 8);
                    rect.x = cell.x + 26;
                    rect.y = label_y3;
                    rect.w = g_unit_list_grid_label_tiles[4].w + 2;
                    rect.h = g_unit_list_grid_label_tiles[4].h;
                    menu_gfx_enqueue_textured_quad(&rect, g_unit_list_grid_label_tiles[4].u,
                        g_unit_list_grid_label_tiles[4].v, color, 0, g_menu_cursor_texture_page,
                        g_menu_alternate_number_clut, 8);
                    g_unit_list_grid_level_number.x = cell.x + 13;
                    g_unit_list_grid_level_number.y = upper_y3;
                    g_unit_list_grid_level_number.value = unit_index;
                    menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_unit_list_grid_level_number);
                    g_unit_list_grid_experience_number.x = cell.x + 45;
                    g_unit_list_grid_experience_number.y = number_y3;
                    g_unit_list_grid_experience_number.value = unit_index;
                    menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_unit_list_grid_experience_number);
                } else if (g_unit_list_stat_mode == MENU_UNIT_READOUT_BRAVE_FAITH) {
                    top = cell_y + 45;
                    rect.x = cell.x - 1;
                    label_y4 = top;
                    upper_y4 = cell_y + 40;
                    rect.y = upper_y4;
                    rect.w = g_unit_list_grid_label_tiles[5].w;
                    rect.h = g_unit_list_grid_label_tiles[5].h;
                    menu_gfx_enqueue_textured_quad(&rect, g_unit_list_grid_label_tiles[5].u,
                        g_unit_list_grid_label_tiles[5].v, color, 0, g_menu_cursor_texture_page,
                        g_menu_alternate_number_clut, 8);
                    rect.x += rect.w - 2;
                    rect.y += 5;
                    rect.w = 3;
                    rect.h = 3;
                    menu_gfx_enqueue_textured_quad(
                        &rect, 0x3B, 0x15, color, 0, g_menu_cursor_texture_page, g_menu_alternate_number_clut, 8);
                    rect.x = cell.x + 26;
                    rect.y = label_y4;
                    rect.w = g_unit_list_grid_label_tiles[7].w;
                    rect.h = g_unit_list_grid_label_tiles[7].h;
                    menu_gfx_enqueue_textured_quad(&rect, g_unit_list_grid_label_tiles[7].u,
                        g_unit_list_grid_label_tiles[7].v, color, 0, g_menu_cursor_texture_page,
                        g_menu_alternate_number_clut, 8);
                    rect.x += rect.w;
                    rect.y += 5;
                    rect.w = 3;
                    rect.h = 3;
                    menu_gfx_enqueue_textured_quad(
                        &rect, 0x3B, 0x15, color, 0, g_menu_cursor_texture_page, g_menu_alternate_number_clut, 8);
                    g_unit_list_grid_brave_number.x = cell.x + 13;
                    g_unit_list_grid_brave_number_wide.y = g_unit_list_grid_brave_number.y = upper_y4;
                    g_unit_list_grid_brave_number_wide.value = g_unit_list_grid_brave_number.value = unit_index;
                    if (g_unit_list_pointers[unit_index]->brave >= 100) {
                        g_unit_list_grid_brave_number_wide.x = cell.x + 10;
                        menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_unit_list_grid_brave_number_wide);
                    } else
                        menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_unit_list_grid_brave_number);
                    lower_y4 = top;
                    g_unit_list_grid_faith_number.x = cell.x + 45;
                    g_unit_list_grid_faith_number_wide.y = g_unit_list_grid_faith_number.y = lower_y4;
                    g_unit_list_grid_faith_number_wide.value = g_unit_list_grid_faith_number.value = unit_index;
                    if (g_unit_list_pointers[unit_index]->faith >= 100) {
                        g_unit_list_grid_faith_number_wide.x = cell.x + 40;
                        menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_unit_list_grid_faith_number_wide);
                    } else
                        menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_unit_list_grid_faith_number);
                }
            }
            unit_sprite_draw(g_unit_list_reorderable_indices[unit], cell.x, cell.y);
            if (g_unit_list_pointers[g_unit_list_reorderable_indices[unit]]->mount_info & UNIT_MOUNT_INFO_FLAG_RIDER) {
                unit_sprite_draw(
                    unit_list_find_by_battle_id(g_unit_list_pointers[g_unit_list_reorderable_indices[unit]]->mount_info
                        - UNIT_MOUNT_INFO_FLAG_RIDER),
                    cell.x, cell.y);
            }
        }
    }
    /* The cursor trail and the background's light ease toward the selected cell. */
    gfx_set_draw_color(&saved_color);
    cursor_trail_update(&g_unit_list_grid_cursor_target);
    focus = &g_unit_list_gfx_falloff_focus_x;
    cursor_x = g_unit_list_grid_cursor_target.x;
    *focus = (*focus * 3 + cursor_x) >> 2;
    row = *focus - cursor_x;
    cursor_y = g_unit_list_grid_cursor_target.y;
    g_unit_list_gfx_falloff_focus_y = (g_unit_list_gfx_falloff_focus_y * 3 + cursor_y) >> 2;
    if (row >= -4 && row <= 4) {
        *focus = cursor_x - 1;
    }
    row = g_unit_list_gfx_falloff_focus_y - cursor_y;
    if (row >= -4 && row <= 4) {
        g_unit_list_gfx_falloff_focus_y = cursor_y - 1;
    }
    unit_list_draw_shaded_rows(g_unit_list_page_offset + g_unit_list_scroll_step);
    return 1;
}

/* Calculate a clamped intensity falloff from BUNIT's animated focal point.
 *
 * Vertical distance is doubled before the Euclidean distance calculation,
 * matching the menu renderer's non-square coordinate scale. `dx` also holds
 * the result. */
static s32 brightness_falloff_calculate(s32 x, s32 y, s32 base_value, s32 minimum) {
    s32 dx = abs(g_unit_list_gfx_falloff_focus_x - x);
    s32 dy;

    dx *= dx;
    dy = abs(g_unit_list_gfx_falloff_focus_y - y);
    dy *= 2;
    dy *= dy;
    dx = base_value - (csqrt((dy + dx) * ONE) >> 12);
    if (dx < minimum) {
        dx = minimum;
    }
    return dx;
}

/* Draw rows of gouraud-textured quads whose vertex brightness falls off with
 * distance from the animated focal point. */
void unit_list_draw_shaded_rows(s32 y) {
    textured_rect_source_t rect;
    /* Unused local bytes and RGB alignment retain the target frame. */
    u8 unused_frame[8];
    rgb24_t rgb[4];
    u8 shades[16][5];
    s32 row;
    s32 col;
    s32 brightness;
    s16 u;
    s32 i;

    row = 0;
    rect.w = 0x40;
    rect.clut = 0x40;
    rect.h = 0x20;
    rect.tpage = 0x20;
    rect.v = 0xD8;
    rect.y = y;
    for (i = 0; y < 0x101; y += 0x20, i++) {
        for (col = 0; col < 5; col++) {
            brightness = brightness_falloff_calculate(col << 6, y, 0xFF, 0x50);
            if (brightness > 0xDC) {
                brightness = 0xDC;
            }
            shades[i][col] = brightness;
        }
    }

    rect.clut = g_unit_list_gfx_background_clut;
    rect.tpage = g_menu_cursor_texture_page;
    for (; rect.y < 0xF0; rect.y += 0x20, row++) {
        if (rect.y >= -0x20) {
            for (col = 0; col < 4; col++) {
                rect.x = g_unit_list_gfx_background_tile_layout[row % 2][col * 2];
                u = g_unit_list_gfx_background_tile_layout[row % 2][col * 2 + 1];
                rect.u = u;
                rgb[0].r = rgb[0].g = rgb[0].b = shades[row][col];
                rgb[1].r = rgb[1].g = rgb[1].b = shades[row][col + 1];
                rgb[2].r = rgb[2].g = rgb[2].b = shades[row + 1][col];
                rgb[3].r = rgb[3].g = rgb[3].b = shades[row + 1][col + 1];
                gfx_enqueue_gouraud_textured_quad(&rect, rgb, 0, u == 0x58);
            }
        }
    }
}

/* Empty the eight cursor trail points (x = -1) and set the texture pages of
 * the cursor sprite and its shadow. */
static void cursor_trail_init(void) {
    s16 term = -1;
    s32 i = 7;
    do {
        g_unit_list_cursor_trail_points[i].x = term;
        i--;
    } while (i >= 0);
    g_unit_list_cursor_shadow_sprite.source.tpage = GetTPage(0, 2, 0x3C0, 0x100);
    g_unit_list_cursor_sprite.source.tpage = GetTPage(0, 1, 0x3C0, 0x100);
}

/* Advance the rhombus-cursor trail toward target and draw it.
 *
 * The newest point eases a quarter of the way toward target each frame and
 * snaps to it when the eased value's magnitude equals the previous value.
 * Each point far enough from the last drawn one is drawn as four oriented
 * quads per cursor layer, faded by g_unit_list_cursor_trail_brightness except for the newest point.
 * abs() matters: GCC expands it to the single-insn abssi2, which keeps each
 * loop body one basic block as in the target. */
static void cursor_trail_update(point16_t* target) {
    rgb24_t rgb;
    s32 i;
    s32 last;
    s32 sum;
    s16 x;
    s16 x2;
    rgb24_t* color;

    for (i = 0; i < 7;) {
        s32 j = i + 1;
        g_unit_list_cursor_trail_points[i].x = g_unit_list_cursor_trail_points[j].x;
        g_unit_list_cursor_trail_points[i].y = g_unit_list_cursor_trail_points[j].y;
        i = j;
    }
    if (g_unit_list_cursor_trail_points[7].x == -1) {
        g_unit_list_cursor_trail_points[7].x = target->x - 0x1F;
        g_unit_list_cursor_trail_points[7].y = target->y - 1;
    } else {
        /* The (c - target) grouping keeps the constant on i * 3, as in the
         * target; (i * 3 - c + target) would be reassociated by fold. */
        i = g_unit_list_cursor_trail_points[7].x;
        g_unit_list_cursor_trail_points[7].x = (i * 3 - (0x1F - target->x)) >> 2;
        if (i == abs(g_unit_list_cursor_trail_points[7].x)) {
            g_unit_list_cursor_trail_points[7].x = target->x - 0x1F;
        }
        i = g_unit_list_cursor_trail_points[7].y;
        g_unit_list_cursor_trail_points[7].y = (i * 3 - (1 - target->y)) >> 2;
        if (i == abs(g_unit_list_cursor_trail_points[7].y)) {
            g_unit_list_cursor_trail_points[7].y = target->y - 1;
        }
    }
    last = 1000;
    for (i = 7; i >= 0; i--) {
        rgb.r = rgb.g = rgb.b = g_unit_list_cursor_trail_brightness[i];
        x = g_unit_list_cursor_trail_points[i].x;
        sum = x + g_unit_list_cursor_trail_points[i].y;
        if (abs(sum - last) < 5) {
            continue;
        }
        last = sum;
        color = 0;
        if (i != 7) {
            color = &rgb;
        }
        if (x == -1) {
            continue;
        }
        g_unit_list_cursor_sprite.position.x = g_unit_list_cursor_shadow_sprite.position.x = x;
        g_unit_list_cursor_sprite.h = g_unit_list_cursor_sprite.source.h = g_unit_list_cursor_shadow_sprite.h
            = g_unit_list_cursor_shadow_sprite.source.h = 0x10;
        g_unit_list_cursor_shadow_sprite.position.y = g_unit_list_cursor_trail_points[i].y;
        g_unit_list_cursor_sprite.position.y = g_unit_list_cursor_trail_points[i].y - 1;
        menu_gfx_enqueue_oriented_textured_quad(&g_unit_list_cursor_shadow_sprite, color, TEXTURE_UV_FLIP_NONE, 1, 3);
        menu_gfx_enqueue_oriented_textured_quad(&g_unit_list_cursor_sprite, color, TEXTURE_UV_FLIP_NONE, 1, 4);
        g_unit_list_cursor_shadow_sprite.position.x += 0x1F;
        g_unit_list_cursor_sprite.position.x += 0x1F;
        menu_gfx_enqueue_oriented_textured_quad(&g_unit_list_cursor_shadow_sprite, color, TEXTURE_UV_FLIP_X, 1, 3);
        menu_gfx_enqueue_oriented_textured_quad(&g_unit_list_cursor_sprite, color, TEXTURE_UV_FLIP_X, 1, 4);
        x2 = g_unit_list_cursor_trail_points[i].x;
        g_unit_list_cursor_sprite.h = g_unit_list_cursor_sprite.source.h = g_unit_list_cursor_shadow_sprite.h
            = g_unit_list_cursor_shadow_sprite.source.h = 0xF;
        g_unit_list_cursor_sprite.position.x = g_unit_list_cursor_shadow_sprite.position.x = x2;
        g_unit_list_cursor_shadow_sprite.position.y = g_unit_list_cursor_trail_points[i].y + 0x10;
        g_unit_list_cursor_sprite.position.y = g_unit_list_cursor_trail_points[i].y + 0xF;
        menu_gfx_enqueue_oriented_textured_quad(&g_unit_list_cursor_shadow_sprite, color, TEXTURE_UV_FLIP_Y, 1, 3);
        menu_gfx_enqueue_oriented_textured_quad(&g_unit_list_cursor_sprite, color, TEXTURE_UV_FLIP_Y, 1, 4);
        g_unit_list_cursor_shadow_sprite.position.x += 0x1F;
        g_unit_list_cursor_sprite.position.x += 0x1F;
        menu_gfx_enqueue_oriented_textured_quad(&g_unit_list_cursor_shadow_sprite, color, TEXTURE_UV_FLIP_XY, 1, 3);
        menu_gfx_enqueue_oriented_textured_quad(&g_unit_list_cursor_sprite, color, TEXTURE_UV_FLIP_XY, 1, 4);
    }
}

/* Draw a unit's current SHP frame at (x, y), then its two name-plate quads.
 *
 * The frame's first byte is taken whole as the part count minus one, where
 * BATTLE keeps only its low three bits; no unit frame on the disc sets the
 * others.
 *
 * Brightness falls off with distance from the animated focal point; the
 * selected unit's plate also pulses. The target keeps two short-lived values
 * in the loop counter and OT-index registers (the falloff result in `i`, the
 * facing quadrant in `value`), so the source reuses those variables. */
static void unit_sprite_draw(s16 unit, s32 x, s32 y) {
    rgb24_t rgb;
    /* The query writes clut/tpage into the quad and two extra 16-bit values after it. */
    struct {
        s16 x;
        s16 y;
        s16 w;
        s16 h;
        s16 u;
        s16 v;
        s16 uw;
        s16 vh;
        sprite_unit_query_t query; /* 0x10 */
    } quad;
    const sprite_frame_t* frame;
    s32 base_y;
    s32 i;
    const sprite_frame_part_t* cells;
    s32 tile;
    s32 dx;
    s32 orientation;
    s32 value;
    s32 size;

    i = brightness_falloff_calculate(x, y, 0xC8, 0x50);
    rgb.r = rgb.g = rgb.b = i > 0x80 ? 0x80 : i;
    frame = sprite_unit_frame_vram_ids_get(g_unit_list_pointers[unit]->runtime_id, &quad.query);
    base_y = (quad.query.graphic_height >> 1) + 0x16;
    /* Retail tests -1, although an absent runtime unit returns 0 and leaves the query unset. */
    if (frame == (const sprite_frame_t*)-1) {
        return;
    }
    cells = frame->parts;
    for (i = 0; i <= frame->part_count_and_rotation; i++) {
        size = (cells[i].attributes >> 10) & (SPRITE_SHP_PART_SIZE_MASK >> 10);
        quad.uw = quad.w = s_unit_list_sprite_cell_sizes[size].width;
        quad.vh = quad.h = s_unit_list_sprite_cell_sizes[size].height;
        value = unit_battle_camera_facing_get(g_unit_list_pointers[unit]->battle_id);
        dx = cells[i].x_shift;
        orientation = TEXTURE_UV_FLIP_NONE;
        if (value >= 2) {
            orientation = TEXTURE_UV_FLIP_X;
            dx = -dx - quad.w;
        }
        /* The (s16) keeps the 0x1C offset on dx instead of letting fold
         * reassociate it onto x. */
        quad.x = x + (s16)(dx + 0x1C);
        quad.y = cells[i].y_shift + (y + base_y);
        if (g_unit_list_pointers[unit]->mount_info & UNIT_MOUNT_INFO_FLAG_RIDER) {
            quad.y -= 8;
        }
        tile = cells[i].attributes & SPRITE_SHP_PART_TILE_MASK;
        quad.u = tile % 32 * 8;
        /* Both tile reads preserve the separate signed divisions. */
        tile = cells[i].attributes & SPRITE_SHP_PART_TILE_MASK;
        quad.v = tile / 32 * 8 + quad.query.graphic_y_offset;
        value = 7;
        if (g_unit_list_pointers[unit]->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT) {
            value = 8;
            if (i >= 3) {
                value = 6;
            }
        }
        menu_gfx_enqueue_oriented_textured_quad((oriented_quad_t*)&quad, &rgb, orientation, 0, value);
    }
    if (g_unit_list_pointers[unit]->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT) {
        return;
    }
    g_unit_list_shadow_sprite.position.x = x + 0x12;
    g_unit_list_shadow_sprite.position.y = y + base_y - 4;
    g_unit_list_shadow_sprite.source.tpage = GetTPage(0, 2, 0x3C0, 0x100);
    menu_gfx_enqueue_oriented_textured_quad(&g_unit_list_shadow_sprite, 0, TEXTURE_UV_FLIP_NONE, 1, 5);
    g_unit_list_dot_sprite.position.x = x;
    g_unit_list_dot_sprite.position.y = y;
    if (g_unit_list_pointers[unit]->team_kind == 1) {
        g_unit_list_dot_sprite.source.clut = 0x3F12;
    } else {
        g_unit_list_dot_sprite.source.clut = 0x3F13;
    }
    g_unit_list_dot_sprite.source.tpage = GetTPage(0, 1, 0x3C0, 0x100);
    if (unit == g_unit_selected_index) {
        g_unit_list_dot_glow_timer += g_unit_list_dot_glow_direction * 2;
        if (g_unit_list_dot_glow_timer > 0x28) {
            g_unit_list_dot_glow_direction = -1;
        }
        if (g_unit_list_dot_glow_timer < -0x28) {
            g_unit_list_dot_glow_direction = 1;
        }
        rgb.r += g_unit_list_dot_glow_timer;
        rgb.g += g_unit_list_dot_glow_timer;
        rgb.b += g_unit_list_dot_glow_timer;
    }
    menu_gfx_enqueue_oriented_textured_quad(&g_unit_list_dot_sprite, &rgb, TEXTURE_UV_FLIP_NONE, 1, 8);
}
