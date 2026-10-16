/* Unit list panels, including stat accessors, list scrolling, the pressable button, the unit grid, falloff shading, the
 * rhombus cursor trail, sprite and name plates, and index lists. */

#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"
#include "fft/event_bunit.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_overlay.h"
#include "fft/menu_panel.h"
#include "fft/menu_script.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

static void bunit_gfx_draw_unit_sprite_and_name_plate(s16 unit, s32 x, s32 y);
static void bunit_gfx_init_rhombus_cursor_tpages(void);
static void bunit_gfx_update_rhombus_cursor_trail(point16_t* target);

void bunit_panel_toggle_equipment_and_numeric_editor_threads(s32 enable) {
    if (enable != 0) {
        if (g_bunit_status_display_flags & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP)) {
            return;
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) != 0) {
            return;
        }
        thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, menu_run_equipment_ability_panel_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, &g_bunit_character_status_frame_config, 0, 0);
        thread_start(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, menu_run_numeric_editor_frame_thread_variant);
        thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, &g_bunit_numeric_editor_thread_params, 0, 0);
    } else {
        menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
    }
}

/* Return the listed unit's current HP, MP, or CT value, as
 * g_bunit_unit_stat_mode selects. For any other mode the value returned is
 * never assigned. */
static s32 bunit_unit_get_current_stat(s32 unit_index) {
    menu_unit_readout_e mode = g_bunit_unit_stat_mode;
    bunit_unit_t* entry;
    s32 value;

    if (mode == MENU_UNIT_READOUT_HP) {
        entry = g_bunit_unit_pointers[unit_index];
        value = entry->gauges[MENU_UNIT_STATUS_GAUGE_HP].value;
    } else if (mode == MENU_UNIT_READOUT_MP) {
        entry = g_bunit_unit_pointers[unit_index];
        value = entry->gauges[MENU_UNIT_STATUS_GAUGE_MP].value;
    } else if (mode == MENU_UNIT_READOUT_CT) {
        entry = g_bunit_unit_pointers[unit_index];
        value = entry->gauges[MENU_UNIT_STATUS_GAUGE_CT].value;
    }
    g_bunit_unit_current_stat_value = value;
    return g_bunit_unit_current_stat_value;
}

/* Return the level of the unit in this unit list slot. */
static s32 bunit_unit_get_level(s32 unit_id) {
    return g_bunit_unit_pointers[unit_id]->level;
}

/* Return the experience of the unit in this unit list slot. */
static s32 bunit_unit_get_experience(s32 unit_id) {
    return g_bunit_unit_pointers[unit_id]->experience;
}

/* Return the Brave of the unit in this unit list slot. */
static s32 bunit_unit_get_brave(s32 unit_id) {
    return g_bunit_unit_pointers[unit_id]->brave;
}

/* Return the Faith of the unit in this unit list slot. */
static s32 bunit_unit_get_faith(s32 unit_id) {
    return g_bunit_unit_pointers[unit_id]->faith;
}

/* Return the selected unit's maximum HP, MP, or CT value. */
static s32 bunit_unit_get_max_stat(s32 unit_index) {
    s32 value;

    if (g_bunit_unit_stat_mode == MENU_UNIT_READOUT_HP) {
        value = g_bunit_unit_pointers[unit_index]->gauges[MENU_UNIT_STATUS_GAUGE_HP].max;
    } else if (g_bunit_unit_stat_mode == MENU_UNIT_READOUT_MP) {
        value = g_bunit_unit_pointers[unit_index]->gauges[MENU_UNIT_STATUS_GAUGE_MP].max;
    } else if (g_bunit_unit_stat_mode == MENU_UNIT_READOUT_CT) {
        value = UNIT_TURN_CT_THRESHOLD;
    }
    g_bunit_unit_max_stat_value = value;
    return g_bunit_unit_max_stat_value;
}

/* Returns 1 when the selected unit is in the two upper visible rows of the list at
 * `page_offset` (after the scroll in progress), which puts the status banner at the bottom
 * of the screen. Returns 0 while browsing units. */
static s32 bunit_menu_is_selected_unit_in_upper_rows(s32 page_offset) {
    s32 in_upper_rows;
    s8 offset = page_offset;
    s32 scroll_step;

    if (g_bunit_unit_browse_enabled == 0) {
        in_upper_rows = 1;
        scroll_step = g_bunit_unit_list_scroll_step;
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
static void bunit_menu_update_unit_list_scroll(s8* step, s8* offset) {
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

/* Draw one slot's span of g_bunit_slot_sprite_entries sprite entries at (x, y).
 *
 * A nonzero `busy` clears the slot's g_menu_button_press_timers phase (CLUT
 * g_menu_cursor_mode1_foreground_clut); a nonzero `pressed` starts it and
 * stores `sound_id` to g_sound_queued_effect_id; otherwise an active
 * phase advances until 6 / event speed frames have passed (CLUT
 * g_menu_cursor_mode0_foreground_clut once idle). While a phase is
 * active the sprite is drawn one pixel lower with CLUT g_menu_button_clut. abs()
 * matters: GCC expands it to the single-insn abssi2, which keeps the entry
 * loop body one basic block as in the target. */
static void bunit_menu_draw_pressable_button(s16 slot, s16 x, s16 y, s16 pressed, s16 busy, u16 sound_id) {
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

/* Set up the unit list page: an empty scrollable list, the row callbacks for
 * current and maximum stats, level, experience, Brave and Faith, and the
 * status banner's position (at the bottom, y 0x90, while the selected unit
 * is in the upper rows). The argument is not read. */
void bunit_menu_init_unit_list(s32 unused_selected_index) {
    s32 ret;

    menu_init_scrollable_list_core_variant(0, 0, 0);
    g_menu_row_callbacks[0].value = bunit_unit_get_current_stat;
    g_menu_row_callbacks[1].value = bunit_unit_get_max_stat;
    g_menu_row_callbacks[4].value = bunit_unit_get_level;
    g_menu_row_callbacks[5].value = bunit_unit_get_experience;
    g_menu_row_callbacks[6].value = bunit_unit_get_brave;
    g_menu_row_callbacks[7].value = bunit_unit_get_faith;
    menu_set_vsync_mode_and_event_speed(0);
    ret = bunit_menu_is_selected_unit_in_upper_rows(g_bunit_unit_list_page_offset);
    g_bunit_status_banner_at_bottom = ret;
    g_bunit_status_banner_at_bottom_latch = ret;
    if (g_bunit_status_banner_at_bottom != 0) {
        g_bunit_status_display_offset_y = 0x90;
    } else {
        g_bunit_status_display_offset_y = 0;
    }
    bunit_gfx_init_rhombus_cursor_tpages();
}

/*
 * Formation-screen unit grid for the unit list menu: handle the stat-mode,
 * browse and banner toggles, step the grid cursor, draw each visible unit
 * panel with its stat readout for g_bunit_unit_stat_mode (layouts 0-2 label,
 * two numbers and gauge; 3 and 4 two labeled numbers), then ease the cursor
 * trail toward the selected cell.
 *
 * Twin of WORLD's world_formation_update_and_draw_unit_grid with world_formation_draw_graphics_below_sprite's panel
 * body written inline. Details the target needs:
 * - `unit` is also the early "enabled" flag, `row` also the stat tile index
 *   and cursor delta, and `top` the button y, highlight flag and panel y, as
 *   in the target's shared registers and stack slots. The cell x is not a
 *   variable: loop.c strength-reduces `col * 62` into the last stack slot.
 * - The busy argument of bunit_menu_draw_pressable_button is an inline `||` each time.
 * - number_y*, label_y* are single-set narrow copies of `top` assigned
 *   before a call; local-alloc sinks each to its use (the target's
 *   `move v0,s2` before the store). Their modes must differ from each other
 *   where two copy the same value.
 * - Single-statement bodies in the column loop are deliberately unbraced
 *   and the g_bunit_unit_browse_enabled and range tests are one condition: every braced
 *   block adds a note to the loop's LUID span, and one more makes loop.c
 *   hoist the shared constant 3 out of the row loop, where it also absorbs
 *   the layout-4 3x3 size that the target keeps local in s0.
 * - Layouts 0-2 read g_bunit_unit_grid_label_tiles as bytes: indexing the struct array changes
 *   the base constants (the target uses g_bunit_unit_grid_label_tiles+2 and +5). This raw
 *   offset is required for the byte-exact match.
 * - The two s16 cursor values give the frame's unreferenced 8-byte slots
 *   together with the two g_unit_selected_index tests; the unused
 *   RECTs reproduce the locals area.
 */
s32 bunit_menu_update_unit_grid(s16 allow_banner_toggle, s16 allow_browse, s16 show_stat_panel, s16 allow_stat_cycle,
    s16 stat_mode, s16 buttons, s16 locked) {
    RECT unused[3];
    RECT cell;
    RECT rect;
    upoint16_t bar_points[4];
    RECT unused2;
    rgb24_t saved_color;
    s32 row;
    s32 col;
    menu_instruction_t* menu;
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
    if (allow_stat_cycle != 0) {
        if (g_bunit_unit_browse_enabled == 0) {
            if (buttons & PSX_PAD_L2) {
                g_bunit_unit_stat_mode = g_bunit_unit_stat_mode == MENU_UNIT_READOUT_HP ? MENU_UNIT_READOUT_BRAVE_FAITH
                                                                                        : g_bunit_unit_stat_mode - 1;
                buttons &= ~PSX_PAD_R2;
                g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
            }
            if (buttons & PSX_PAD_R2) {
                g_bunit_unit_stat_mode = g_bunit_unit_stat_mode == MENU_UNIT_READOUT_BRAVE_FAITH
                    ? MENU_UNIT_READOUT_HP
                    : g_bunit_unit_stat_mode + 1;
                g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
            }
        }
    }
    if (allow_stat_cycle == 0) {
        g_bunit_unit_stat_mode = stat_mode;
    }
    unit = 1;
    if (allow_browse != 0 && bunit_input_get_lock_timer() == 0
        && thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) == 0
        && !(g_bunit_status_display_flags & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP))
        && g_bunit_status_banner_slide_step == 0 && g_bunit_unit_list_scroll_step == 0) {
        if (buttons & PSX_PAD_CIRCLE) {
            if (g_bunit_unit_browse_enabled == 0 && thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0) {
                g_bunit_unit_browse_enabled = 1;
                g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            }
        } else if (buttons & PSX_PAD_CROSS) {
            if (g_bunit_unit_browse_enabled != 0) {
                unit = 0;
                g_sound_queued_effect_id = MAIN_SFX_CANCEL;
                g_bunit_unit_browse_enabled = 0;
            }
        }
    }
    if (g_bunit_unit_browse_enabled != 0) {
        buttons &= 0xFFF;
    }
    if (allow_banner_toggle != 0 && g_bunit_unit_browse_enabled == 0 && (buttons & PSX_PAD_SQUARE)
        && bunit_menu_is_unit_list_scroll_needed() == 0 && thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0
        && thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) == 0
        && !(g_bunit_status_display_flags & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP))) {
        g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
        g_bunit_status_banner_enabled = g_bunit_status_banner_enabled == 0;
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0 && g_bunit_help_screen_id == MENU_GRID_HELP_NONE
        && gfx_get_fade_state() == 0) {
        g_bunit_unit_grid_selected_cell = menu_update_wrapped_grid_selection(
            4, 5, g_bunit_unit_reorderable_count - 1, g_bunit_unit_grid_selected_cell, buttons);
        g_unit_selected_index = g_bunit_unit_reorderable_indices[g_bunit_unit_grid_selected_cell];
    }
    bunit_menu_update_unit_list_scroll(&g_bunit_unit_list_scroll_step, &g_bunit_unit_list_page_offset);
    if (g_bunit_unit_browse_enabled != 0) {
        if (buttons & PSX_PAD_L1) {
            g_unit_selected_index
                = g_unit_selected_index == 0 ? g_bunit_unit_list_count - 1 : g_unit_selected_index - 1;
        } else if (buttons & PSX_PAD_R1) {
            g_unit_selected_index
                = g_unit_selected_index != g_bunit_unit_list_count - 1 ? g_unit_selected_index + 1 : 0;
        }
        g_bunit_unit_grid_selected_cell = g_bunit_unit_list_index_map[g_unit_selected_index];
        bunit_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_L1, 0xC, 0xE, buttons & PSX_PAD_L1,
            bunit_menu_is_active_or_transitioning() != 0 || g_bunit_help_screen_id != MENU_GRID_HELP_NONE,
            MAIN_SFX_PAGE_SWITCH);
        bunit_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_R1, 0xE0, 0xE, buttons & PSX_PAD_R1,
            bunit_menu_is_active_or_transitioning() != 0 || g_bunit_help_screen_id != MENU_GRID_HELP_NONE,
            MAIN_SFX_PAGE_SWITCH);
    }
    if (g_bunit_status_banner_slide_step == 0 && bunit_input_get_lock_timer() == 0 && unit != 0 && show_stat_panel != 0
        && g_bunit_status_banner_enabled != 0 && g_bunit_unit_browse_enabled == 0) {
        top = 0xF;
        if (g_bunit_status_banner_at_bottom != 0) {
            menu = &g_bunit_unit_grid_stat_bar_script_top;
            quads = g_bunit_unit_grid_label_tiles;
        } else {
            top = 0xDB;
            menu = &g_bunit_unit_grid_stat_bar_script_bottom;
            quads = g_bunit_unit_grid_label_tiles_bottom;
        }
        bunit_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_L2, 0x26, top, buttons & PSX_PAD_L2,
            bunit_menu_is_active_or_transitioning() != 0 || g_bunit_help_screen_id != MENU_GRID_HELP_NONE,
            MAIN_SFX_PAGE_SWITCH);
        bunit_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_R2, 0xC2, top, buttons & PSX_PAD_R2,
            bunit_menu_is_active_or_transitioning() != 0 || g_bunit_help_screen_id != MENU_GRID_HELP_NONE,
            MAIN_SFX_PAGE_SWITCH);
        menu_run_script_with_palette_mode_variant(menu, 0, 0);
        menu_gfx_enqueue_textured_quad_list(quads, 0, 0, g_menu_cursor_texture_page, g_menu_text_clut_2_mode0, 0xB, 8);
        mode = g_bunit_unit_stat_mode;
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
    row = 0;
    g_bunit_status_banner_at_bottom = bunit_menu_is_selected_unit_in_upper_rows(g_bunit_unit_list_page_offset);
    y = 0x24 + g_bunit_unit_list_page_offset + g_bunit_unit_list_scroll_step;
    cell.w = 0x38;
    cell.h = 0x30;
    gfx_set_otag_index(8);
    unit = 0;
    color = menu_gfx_get_draw_color_variant();
    saved_color.r = color->r;
    saved_color.g = color->g;
    saved_color.b = color->b;
    for (; row < 5; row++, y += 0x3C) {
        top = 0;
        if (g_bunit_status_banner_enabled != 0) {
            if (g_bunit_status_banner_at_bottom != 0) {
                top = y > 0x8F;
            } else if (y < 0x47) {
                top = 1;
            }
        }
        gfx_set_draw_color_variant(top ? &g_bunit_unit_grid_dimmed_color : &g_bunit_unit_grid_normal_color);
        for (col = 0; col < 4; col++, unit++) {
            if (unit >= g_bunit_unit_reorderable_count)
                break;
            cell.x = col * 62 + 6;
            cell.y = y;
            if (unit == g_bunit_unit_grid_selected_cell) {
                g_bunit_unit_grid_cursor_target.x = col * 62 + 0x22;
                g_bunit_unit_grid_cursor_target.y = y + 0x19;
            }
            if (y < -0x3C || y >= 0xF0)
                continue;
            cell_y = cell.y;
            unit_index = g_bunit_unit_reorderable_indices[unit];
            top = cell_y + 40;
            if (g_bunit_unit_browse_enabled == 0 && top < 240) {
                if (g_bunit_unit_stat_mode < 3) {
                    number_y = top;
                    rect.x = cell.x;
                    rect.y = top;
                    rect.w = ((u8*)g_bunit_unit_grid_label_tiles)[g_bunit_unit_stat_mode * 6 + 2];
                    rect.h = ((u8*)g_bunit_unit_grid_label_tiles)[g_bunit_unit_stat_mode * 6 + 3];
                    menu_gfx_enqueue_textured_quad(&rect,
                        ((u8*)g_bunit_unit_grid_label_tiles)[g_bunit_unit_stat_mode * 6 + 4],
                        ((u8*)g_bunit_unit_grid_label_tiles)[g_bunit_unit_stat_mode * 6 + 5], color, 0,
                        g_menu_cursor_texture_page, g_menu_alternate_number_clut, 8);
                    g_bunit_unit_grid_max_stat_number.x = cell.x + 40;
                    g_bunit_unit_grid_max_stat_number.y = cell_y + 45;
                    g_bunit_unit_grid_max_stat_number.value = unit_index;
                    menu_script_draw_formatted_number_variant((menu_instruction_t*)&g_bunit_unit_grid_max_stat_number);
                    g_bunit_unit_grid_stat_number.x = cell.x + 20;
                    g_bunit_unit_grid_stat_number.y = number_y;
                    g_bunit_unit_grid_stat_number.value = unit_index;
                    menu_script_draw_formatted_number_variant((menu_instruction_t*)&g_bunit_unit_grid_stat_number);
                    rect.x = cell.x + 35;
                    rect.y = top + 5;
                    rect.w = 6;
                    rect.h = 11;
                    menu_gfx_enqueue_textured_quad(
                        &rect, 0xB4, 0x10, color, 0, g_menu_atlas_texture_page, g_menu_alternate_number_clut, 8);
                    if (g_bunit_unit_stat_mode == MENU_UNIT_READOUT_HP) {
                        clut = g_bunit_unit_grid_hp_gauge_clut;
                        gradient = g_bunit_unit_grid_hp_gauge_colors;
                    } else if (g_bunit_unit_stat_mode == MENU_UNIT_READOUT_MP) {
                        clut = g_bunit_unit_grid_mp_gauge_clut;
                        gradient = g_bunit_unit_grid_mp_gauge_colors;
                    } else {
                        clut = g_bunit_unit_grid_ct_gauge_clut;
                        gradient = g_bunit_unit_grid_ct_gauge_colors;
                    }
                    rect.x = cell.x;
                    rect.y = top + 8;
                    rect.w = 0x25;
                    rect.h = 3;
                    menu_gfx_enqueue_textured_quad(&rect, 0xD8, 0xCA, color, 0, g_menu_cursor_texture_page, clut, 7);
                    bar_points[0].x = g_bunit_unit_grid_gauge_offsets[0] + cell.x;
                    bar_points[0].y = g_bunit_unit_grid_gauge_offsets[1] + top + 8;
                    if (g_bunit_unit_max_stat_value != 0) {
                        width = (g_bunit_unit_current_stat_value << 5) / g_bunit_unit_max_stat_value;
                    } else {
                        width = 0;
                    }
                    bar_points[3].x = bar_points[1].x = width;
                    bar_points[1].x += bar_points[0].x;
                    bar_points[1].y = g_bunit_unit_grid_gauge_offsets[1] + top + 8;
                    bar_points[2].x = g_bunit_unit_grid_gauge_offsets[2] + cell.x;
                    bar_points[2].y = g_bunit_unit_grid_gauge_offsets[3] + top + 8;
                    bar_points[3].x += bar_points[2].x;
                    bar_points[3].y = g_bunit_unit_grid_gauge_offsets[3] + top + 8;
                    gfx_enqueue_gouraud_quad(bar_points, gradient, 0, 8);
                } else if (g_bunit_unit_stat_mode == MENU_UNIT_READOUT_LEVEL_EXP) {
                    top = cell_y + 45;
                    rect.x = cell.x - 1;
                    label_y3 = top;
                    number_y3 = top;
                    upper_y3 = cell_y + 40;
                    rect.y = upper_y3;
                    rect.w = g_bunit_unit_grid_label_tiles[3].w + 3;
                    rect.h = g_bunit_unit_grid_label_tiles[3].h + 1;
                    menu_gfx_enqueue_textured_quad(&rect, g_bunit_unit_grid_label_tiles[3].u,
                        g_bunit_unit_grid_label_tiles[3].v, color, 0, g_menu_cursor_texture_page,
                        g_menu_alternate_number_clut, 8);
                    rect.x = cell.x + 26;
                    rect.y = label_y3;
                    rect.w = g_bunit_unit_grid_label_tiles[4].w + 2;
                    rect.h = g_bunit_unit_grid_label_tiles[4].h;
                    menu_gfx_enqueue_textured_quad(&rect, g_bunit_unit_grid_label_tiles[4].u,
                        g_bunit_unit_grid_label_tiles[4].v, color, 0, g_menu_cursor_texture_page,
                        g_menu_alternate_number_clut, 8);
                    g_bunit_unit_grid_level_number.x = cell.x + 13;
                    g_bunit_unit_grid_level_number.y = upper_y3;
                    g_bunit_unit_grid_level_number.value = unit_index;
                    menu_script_draw_formatted_number_variant((menu_instruction_t*)&g_bunit_unit_grid_level_number);
                    g_bunit_unit_grid_experience_number.x = cell.x + 45;
                    g_bunit_unit_grid_experience_number.y = number_y3;
                    g_bunit_unit_grid_experience_number.value = unit_index;
                    menu_script_draw_formatted_number_variant(
                        (menu_instruction_t*)&g_bunit_unit_grid_experience_number);
                } else if (g_bunit_unit_stat_mode == MENU_UNIT_READOUT_BRAVE_FAITH) {
                    top = cell_y + 45;
                    rect.x = cell.x - 1;
                    label_y4 = top;
                    upper_y4 = cell_y + 40;
                    rect.y = upper_y4;
                    rect.w = g_bunit_unit_grid_label_tiles[5].w;
                    rect.h = g_bunit_unit_grid_label_tiles[5].h;
                    menu_gfx_enqueue_textured_quad(&rect, g_bunit_unit_grid_label_tiles[5].u,
                        g_bunit_unit_grid_label_tiles[5].v, color, 0, g_menu_cursor_texture_page,
                        g_menu_alternate_number_clut, 8);
                    rect.x += rect.w - 2;
                    rect.y += 5;
                    rect.w = 3;
                    rect.h = 3;
                    menu_gfx_enqueue_textured_quad(
                        &rect, 0x3B, 0x15, color, 0, g_menu_cursor_texture_page, g_menu_alternate_number_clut, 8);
                    rect.x = cell.x + 26;
                    rect.y = label_y4;
                    rect.w = g_bunit_unit_grid_label_tiles[7].w;
                    rect.h = g_bunit_unit_grid_label_tiles[7].h;
                    menu_gfx_enqueue_textured_quad(&rect, g_bunit_unit_grid_label_tiles[7].u,
                        g_bunit_unit_grid_label_tiles[7].v, color, 0, g_menu_cursor_texture_page,
                        g_menu_alternate_number_clut, 8);
                    rect.x += rect.w;
                    rect.y += 5;
                    rect.w = 3;
                    rect.h = 3;
                    menu_gfx_enqueue_textured_quad(
                        &rect, 0x3B, 0x15, color, 0, g_menu_cursor_texture_page, g_menu_alternate_number_clut, 8);
                    g_bunit_unit_grid_brave_number.x = cell.x + 13;
                    g_bunit_unit_grid_brave_number_wide.y = g_bunit_unit_grid_brave_number.y = upper_y4;
                    g_bunit_unit_grid_brave_number_wide.value = g_bunit_unit_grid_brave_number.value = unit_index;
                    if (g_bunit_unit_pointers[unit_index]->brave >= 100) {
                        g_bunit_unit_grid_brave_number_wide.x = cell.x + 10;
                        menu_script_draw_formatted_number_variant(
                            (menu_instruction_t*)&g_bunit_unit_grid_brave_number_wide);
                    } else
                        menu_script_draw_formatted_number_variant((menu_instruction_t*)&g_bunit_unit_grid_brave_number);
                    lower_y4 = top;
                    g_bunit_unit_grid_faith_number.x = cell.x + 45;
                    g_bunit_unit_grid_faith_number_wide.y = g_bunit_unit_grid_faith_number.y = lower_y4;
                    g_bunit_unit_grid_faith_number_wide.value = g_bunit_unit_grid_faith_number.value = unit_index;
                    if (g_bunit_unit_pointers[unit_index]->faith >= 100) {
                        g_bunit_unit_grid_faith_number_wide.x = cell.x + 40;
                        menu_script_draw_formatted_number_variant(
                            (menu_instruction_t*)&g_bunit_unit_grid_faith_number_wide);
                    } else
                        menu_script_draw_formatted_number_variant((menu_instruction_t*)&g_bunit_unit_grid_faith_number);
                }
            }
            bunit_gfx_draw_unit_sprite_and_name_plate(g_bunit_unit_reorderable_indices[unit], cell.x, cell.y);
            if (g_bunit_unit_pointers[g_bunit_unit_reorderable_indices[unit]]->mount_info
                & BATTLE_MOUNT_INFO_FLAG_RIDER) {
                bunit_gfx_draw_unit_sprite_and_name_plate(
                    bunit_unit_find_index_by_battle_id(
                        g_bunit_unit_pointers[g_bunit_unit_reorderable_indices[unit]]->mount_info
                        - BATTLE_MOUNT_INFO_FLAG_RIDER),
                    cell.x, cell.y);
            }
        }
    }
    gfx_set_draw_color_variant(&saved_color);
    bunit_gfx_update_rhombus_cursor_trail(&g_bunit_unit_grid_cursor_target);
    focus = &g_bunit_gfx_falloff_focus_x;
    cursor_x = g_bunit_unit_grid_cursor_target.x;
    *focus = (*focus * 3 + cursor_x) >> 2;
    row = *focus - cursor_x;
    cursor_y = g_bunit_unit_grid_cursor_target.y;
    g_bunit_gfx_falloff_focus_y = (g_bunit_gfx_falloff_focus_y * 3 + cursor_y) >> 2;
    if (row >= -4 && row <= 4) {
        *focus = cursor_x - 1;
    }
    row = g_bunit_gfx_falloff_focus_y - cursor_y;
    if (row >= -4 && row <= 4) {
        g_bunit_gfx_falloff_focus_y = cursor_y - 1;
    }
    bunit_gfx_draw_falloff_shaded_rows(g_bunit_unit_list_page_offset + g_bunit_unit_list_scroll_step);
    return 1;
}

/* Calculate a clamped intensity falloff from BUNIT's animated focal point.
 *
 * Vertical distance is doubled before the Euclidean distance calculation,
 * matching the menu renderer's non-square coordinate scale. `dx` also holds
 * the result. */
static s32 bunit_gfx_calculate_distance_falloff(s32 x, s32 y, s32 base_value, s32 minimum) {
    s32 dx = abs(g_bunit_gfx_falloff_focus_x - x);
    s32 dy;

    dx *= dx;
    dy = abs(g_bunit_gfx_falloff_focus_y - y);
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
void bunit_gfx_draw_falloff_shaded_rows(s32 y) {
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
            brightness = bunit_gfx_calculate_distance_falloff(col << 6, y, 0xFF, 0x50);
            if (brightness > 0xDC) {
                brightness = 0xDC;
            }
            shades[i][col] = brightness;
        }
    }

    rect.clut = g_bunit_gfx_background_clut;
    rect.tpage = g_menu_cursor_texture_page;
    for (; rect.y < 0xF0; rect.y += 0x20, row++) {
        if (rect.y >= -0x20) {
            for (col = 0; col < 4; col++) {
                rect.x = g_bunit_gfx_background_tile_layout[row % 2][col * 2];
                u = g_bunit_gfx_background_tile_layout[row % 2][col * 2 + 1];
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
static void bunit_gfx_init_rhombus_cursor_tpages(void) {
    s16 term = -1;
    s32 i = 7;
    do {
        g_bunit_cursor_trail_points[i].x = term;
        i--;
    } while (i >= 0);
    g_bunit_cursor_shadow_sprite.source.tpage = GetTPage(0, 2, 0x3C0, 0x100);
    g_bunit_cursor_sprite.source.tpage = GetTPage(0, 1, 0x3C0, 0x100);
}

/* Advance the rhombus-cursor trail toward target and draw it.
 *
 * The newest point eases a quarter of the way toward target each frame and
 * snaps to it when the eased value's magnitude equals the previous value.
 * Each point far enough from the last drawn one is drawn as four oriented
 * quads per cursor layer, faded by g_bunit_cursor_trail_brightness except for the newest point.
 * abs() matters: GCC expands it to the single-insn abssi2, which keeps each
 * loop body one basic block as in the target. */
static void bunit_gfx_update_rhombus_cursor_trail(point16_t* target) {
    rgb24_t rgb;
    s32 i;
    s32 last;
    s32 sum;
    s16 x;
    s16 x2;
    rgb24_t* color;

    for (i = 0; i < 7;) {
        s32 j = i + 1;
        g_bunit_cursor_trail_points[i].x = g_bunit_cursor_trail_points[j].x;
        g_bunit_cursor_trail_points[i].y = g_bunit_cursor_trail_points[j].y;
        i = j;
    }
    if (g_bunit_cursor_trail_points[7].x == -1) {
        g_bunit_cursor_trail_points[7].x = target->x - 0x1F;
        g_bunit_cursor_trail_points[7].y = target->y - 1;
    } else {
        /* The (c - target) grouping keeps the constant on i * 3, as in the
         * target; (i * 3 - c + target) would be reassociated by fold. */
        i = g_bunit_cursor_trail_points[7].x;
        g_bunit_cursor_trail_points[7].x = (i * 3 - (0x1F - target->x)) >> 2;
        if (i == abs(g_bunit_cursor_trail_points[7].x)) {
            g_bunit_cursor_trail_points[7].x = target->x - 0x1F;
        }
        i = g_bunit_cursor_trail_points[7].y;
        g_bunit_cursor_trail_points[7].y = (i * 3 - (1 - target->y)) >> 2;
        if (i == abs(g_bunit_cursor_trail_points[7].y)) {
            g_bunit_cursor_trail_points[7].y = target->y - 1;
        }
    }
    last = 1000;
    for (i = 7; i >= 0; i--) {
        rgb.r = rgb.g = rgb.b = g_bunit_cursor_trail_brightness[i];
        x = g_bunit_cursor_trail_points[i].x;
        sum = x + g_bunit_cursor_trail_points[i].y;
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
        g_bunit_cursor_sprite.position.x = g_bunit_cursor_shadow_sprite.position.x = x;
        g_bunit_cursor_sprite.h = g_bunit_cursor_sprite.source.h = g_bunit_cursor_shadow_sprite.h
            = g_bunit_cursor_shadow_sprite.source.h = 0x10;
        g_bunit_cursor_shadow_sprite.position.y = g_bunit_cursor_trail_points[i].y;
        g_bunit_cursor_sprite.position.y = g_bunit_cursor_trail_points[i].y - 1;
        menu_gfx_enqueue_oriented_textured_quad(&g_bunit_cursor_shadow_sprite, color, TEXTURE_UV_FLIP_NONE, 1, 3);
        menu_gfx_enqueue_oriented_textured_quad(&g_bunit_cursor_sprite, color, TEXTURE_UV_FLIP_NONE, 1, 4);
        g_bunit_cursor_shadow_sprite.position.x += 0x1F;
        g_bunit_cursor_sprite.position.x += 0x1F;
        menu_gfx_enqueue_oriented_textured_quad(&g_bunit_cursor_shadow_sprite, color, TEXTURE_UV_FLIP_X, 1, 3);
        menu_gfx_enqueue_oriented_textured_quad(&g_bunit_cursor_sprite, color, TEXTURE_UV_FLIP_X, 1, 4);
        x2 = g_bunit_cursor_trail_points[i].x;
        g_bunit_cursor_sprite.h = g_bunit_cursor_sprite.source.h = g_bunit_cursor_shadow_sprite.h
            = g_bunit_cursor_shadow_sprite.source.h = 0xF;
        g_bunit_cursor_sprite.position.x = g_bunit_cursor_shadow_sprite.position.x = x2;
        g_bunit_cursor_shadow_sprite.position.y = g_bunit_cursor_trail_points[i].y + 0x10;
        g_bunit_cursor_sprite.position.y = g_bunit_cursor_trail_points[i].y + 0xF;
        menu_gfx_enqueue_oriented_textured_quad(&g_bunit_cursor_shadow_sprite, color, TEXTURE_UV_FLIP_Y, 1, 3);
        menu_gfx_enqueue_oriented_textured_quad(&g_bunit_cursor_sprite, color, TEXTURE_UV_FLIP_Y, 1, 4);
        g_bunit_cursor_shadow_sprite.position.x += 0x1F;
        g_bunit_cursor_sprite.position.x += 0x1F;
        menu_gfx_enqueue_oriented_textured_quad(&g_bunit_cursor_shadow_sprite, color, TEXTURE_UV_FLIP_XY, 1, 3);
        menu_gfx_enqueue_oriented_textured_quad(&g_bunit_cursor_sprite, color, TEXTURE_UV_FLIP_XY, 1, 4);
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
static void bunit_gfx_draw_unit_sprite_and_name_plate(s16 unit, s32 x, s32 y) {
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
        battle_unit_sprite_query_t query; /* 0x10 */
    } quad;
    battle_gfx_source_frame_t* frame;
    s32 base_y;
    s32 i;
    battle_gfx_source_part_t* cells;
    s32 tile;
    s32 dx;
    s32 orientation;
    s32 value;
    s32 size;

    i = bunit_gfx_calculate_distance_falloff(x, y, 0xC8, 0x50);
    rgb.r = rgb.g = rgb.b = i > 0x80 ? 0x80 : i;
    frame = battle_gfx_get_unit_sprite_frame_and_vram_ids(g_bunit_unit_pointers[unit]->runtime_id, &quad.query);
    base_y = (quad.query.graphic_height >> 1) + 0x16;
    if (frame == (battle_gfx_source_frame_t*)-1) {
        return;
    }
    cells = frame->parts;
    for (i = 0; i <= frame->part_count_and_rotation; i++) {
        size = (cells[i].attributes >> 10) & (BATTLE_SHP_PART_SIZE_MASK >> 10);
        quad.uw = quad.w = g_bunit_sprite_cell_sizes[size][0];
        quad.vh = quad.h = g_bunit_sprite_cell_sizes[size][1];
        value = battle_unit_get_camera_facing_quadrant_by_battle_id(g_bunit_unit_pointers[unit]->battle_id);
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
        if (g_bunit_unit_pointers[unit]->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) {
            quad.y -= 8;
        }
        tile = cells[i].attributes & BATTLE_SHP_PART_TILE_MASK;
        quad.u = tile % 32 * 8;
        /* Both tile reads preserve the separate signed divisions. */
        tile = cells[i].attributes & BATTLE_SHP_PART_TILE_MASK;
        quad.v = tile / 32 * 8 + quad.query.graphic_y_offset;
        value = 7;
        if (g_bunit_unit_pointers[unit]->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
            value = 8;
            if (i >= 3) {
                value = 6;
            }
        }
        menu_gfx_enqueue_oriented_textured_quad((oriented_quad_t*)&quad, &rgb, orientation, 0, value);
    }
    if (g_bunit_unit_pointers[unit]->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
        return;
    }
    g_bunit_unit_shadow_sprite.position.x = x + 0x12;
    g_bunit_unit_shadow_sprite.position.y = y + base_y - 4;
    g_bunit_unit_shadow_sprite.source.tpage = GetTPage(0, 2, 0x3C0, 0x100);
    menu_gfx_enqueue_oriented_textured_quad(&g_bunit_unit_shadow_sprite, 0, TEXTURE_UV_FLIP_NONE, 1, 5);
    g_bunit_unit_dot_sprite.position.x = x;
    g_bunit_unit_dot_sprite.position.y = y;
    if (g_bunit_unit_pointers[unit]->team_kind == 1) {
        g_bunit_unit_dot_sprite.source.clut = 0x3F12;
    } else {
        g_bunit_unit_dot_sprite.source.clut = 0x3F13;
    }
    g_bunit_unit_dot_sprite.source.tpage = GetTPage(0, 1, 0x3C0, 0x100);
    if (unit == g_unit_selected_index) {
        g_bunit_unit_dot_glow_timer += g_bunit_unit_dot_glow_direction * 2;
        if (g_bunit_unit_dot_glow_timer > 0x28) {
            g_bunit_unit_dot_glow_direction = -1;
        }
        if (g_bunit_unit_dot_glow_timer < -0x28) {
            g_bunit_unit_dot_glow_direction = 1;
        }
        rgb.r += g_bunit_unit_dot_glow_timer;
        rgb.g += g_bunit_unit_dot_glow_timer;
        rgb.b += g_bunit_unit_dot_glow_timer;
    }
    menu_gfx_enqueue_oriented_textured_quad(&g_bunit_unit_dot_sprite, &rgb, TEXTURE_UV_FLIP_NONE, 1, 8);
}

/* Build the unit-list selection-to-record index map.
 *
 * Linked entries encode a roster ID in their formation-order key; ordinary
 * entries retain their compact order among directly reorderable units.
 */
void bunit_unit_build_list_index_map(void) {
    s32 reorderable_index = 0;
    s32 unit_index;

    for (unit_index = 0; unit_index < g_bunit_unit_list_count; unit_index++) {
        bunit_unit_t* unit = g_bunit_unit_pointers[unit_index];

        if (!(unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)) {
            g_bunit_unit_list_index_map[unit_index] = reorderable_index;
            reorderable_index++;
        } else {
            g_bunit_unit_list_index_map[unit_index]
                = bunit_unit_find_index_by_battle_id((unit->mount_info - BATTLE_MOUNT_INFO_FLAG_MOUNT));
        }
    }
}

/* Build the unit-index list used by the formation reorder menu.
 *
 * Entries tagged with bit 0x40 in their formation-order key are resolved separately
 * through their encoded roster ID, so they are omitted from this direct list.
 */
s32 bunit_unit_build_reorderable_index_list(void) {
    s32 count = 0;
    s32 i;

    for (i = 0; i < g_bunit_unit_list_count; i++) {
        if ((g_bunit_unit_pointers[i]->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) == 0) {
            g_bunit_unit_reorderable_indices[count] = i;
            count++;
        }
    }
    return count;
}

/*
 * Per-frame handler of the unit list's sort-order menu.
 *
 * The menu lists the 11 sort keys in their current priority order. Choosing
 * a row moves that key to the front, shifting the rows above it down one,
 * then stores the order back and re-sorts and rebuilds the unit list. The
 * handler returns to the unit list when the menu thread ends.
 */
void bunit_menu_run_reorder_list(void) {
    s16 opts[16];
    s16* src;
    s16* dst;
    s32 i;
    s16 result;
    s32 sel;
    s32 keep;

    if (g_bunit_menu_reorder_list_initialized == 0) {
        g_bunit_menu_reorder_list_index = 0;
        bunit_unit_copy_orders_to_reorder_list(1, g_bunit_menu_reorder_list_entries);
        i = 0;
        src = g_bunit_menu_reorder_list_entries;
        dst = opts;
        do {
            *dst = *src + 1;
            src++;
            i++;
            dst++;
        } while (i < 11);
        opts[11] = -1;
        text_concatenate_entries(g_bunit_text_menu_section, g_bunit_text_reorder_list_buffer, opts, 1);
        g_bunit_menu_reorder_list_initialized = 1;
    }

    if (bunit_menu_run_descriptor_thread(BUNIT_THREAD_SLOT_REORDER_MENU, g_bunit_menu_reorder_list_desc) == 0) {
        g_bunit_menu_reorder_list_initialized = 0;
        g_bunit_menu_current_menu = BUNIT_MENU_UNIT_LIST;
    }

    g_text_selection_id = g_bunit_menu_reorder_list_entries[g_bunit_menu_reorder_list_index] + 0x1002;
    result = g_bunit_menu_reorder_result;
    if (result != -1) {
        sel = result;
        keep = g_bunit_menu_reorder_list_entries[sel];
        for (sel--; sel >= 0; sel--) {
            g_bunit_menu_reorder_list_entries[sel + 1] = g_bunit_menu_reorder_list_entries[sel];
        }
        g_bunit_menu_reorder_list_entries[0] = keep;
        bunit_menu_reset_results();
        i = 0;
        src = g_bunit_menu_reorder_list_entries;
        dst = opts;
        do {
            *dst = *src + 1;
            src++;
            i++;
            dst++;
        } while (i < 11);
        opts[11] = -1;
        text_concatenate_entries(g_bunit_text_menu_section, g_bunit_text_reorder_list_buffer, opts, 1);
        g_bunit_menu_reorder_list_redraw_request = 1;
        bunit_unit_copy_reorder_list_to_orders(1, g_bunit_menu_reorder_list_entries);
        bunit_unit_sort_index_list_by_order_keys(1);
        bunit_unit_build_reorderable_index_list();
        bunit_unit_build_list_index_map();
    }
}
