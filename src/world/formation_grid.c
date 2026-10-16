/* The formation unit grid, including the stat and change banner panel toggles, unit stat accessors, egg hatch progress,
 * grid rows, scrolling, the cursor, transitions, portrait slides and sprites, and graphics drawn below the sprite. */

#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_help.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_overlay.h"
#include "fft/menu_panel.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/menu_status.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "fft/world.h"
#include "fft/world_formation.h"
#include "fft/world_gfx.h"
#include "fft/world_menu.h"
#include "fft/world_script.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

static s32 formation_cursor_distance_falloff(s32 x, s32 y, s32 base, s32 floor);
static void formation_draw_cursor_trail(point16_t* cursor);
static void formation_draw_egg_sprite(
    point16_t* pos, s32 palette, const rgb24_t* rgb, s32 hatch_countdown, s32 ot_index);
static void formation_draw_unit_readout(
    s16 unit_index, s32 mode, s32 x, s32 y, rgb24_t* color, s32 (*get_value)(s32), s32 enabled, s32 priority);
static void formation_reset_slots_and_frame_tpages(void);

/* Show the stat preview window (starting its thread unless the status panel is sliding
 * or the thread already runs) or ask it to close, and record the state as the numeric
 * panel style. */
void formation_toggle_preview_stats_window(s32 flag) {
    s32 stored = flag;

    if (stored != 0) {
        stored = 1;
        if (!(g_unit_panel_status_display_thread_params.flags
                & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP))
            && thread_is_running(SCREEN_THREAD_SLOT_PREVIEW_STATS) == 0) {
            thread_start(SCREEN_THREAD_SLOT_PREVIEW_STATS, unit_panel_numbers_thread);
            thread_set_parameters(
                SCREEN_THREAD_SLOT_PREVIEW_STATS, (s32)&g_unit_panel_preview_stats_thread_params, 0, 0);
        }
    } else {
        menu_request_thread_stop(SCREEN_THREAD_SLOT_PREVIEW_STATS);
    }
    g_unit_panel_numeric_panel_params.style = stored;
    g_formation_preview_stats_window_active = stored;
}

/* With `enable` set, start unit_panel_ability_thread in the equip/ability panel slot (with
 * g_formation_ability_panel_thread_params) unless that slot or the character status slot already
 * runs, show the unit banner and open the numeric panel; otherwise stop the panel and the
 * numeric panel. */
void formation_toggle_ability_panel(s32 enable) {
    if (enable != 0) {
        enable = 1;
        if (thread_is_running(SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL) == 0
            && thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0) {
            thread_start(SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL, unit_panel_ability_thread);
            thread_set_parameters(
                SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL, (s32)&g_formation_ability_panel_thread_params, 0, 0);
        }
        g_formation_unit_banner_enabled = 1;
        unit_panel_toggle_numbers_thread(enable);
    } else {
        menu_request_thread_stop(SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
    }
}

/* With enabled set, open the equipment/ability panel (unless the status panel is sliding or a
 * panel thread already runs) and the numeric panel; enabled 0 does nothing. */
void formation_open_equipment_panel(s32 enabled) {
    if (enabled == 0) {
        return;
    }
    enabled = 1;
    if (g_unit_panel_status_display_thread_params.flags & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP)) {
        return;
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0
        && thread_is_running(SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL) == 0) {
        thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, unit_panel_equipment_ability_thread);
        thread_set_parameters(
            THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_formation_equipment_panel_thread_params, 0, 0);
    }
    unit_panel_toggle_numbers_thread(enabled);
}

/* Request a redraw from all eight formation panel threads. */
void formation_redraw_panels(void) {
    g_unit_panel_status_display_thread_params.redraw_request = 1;
    g_unit_panel_comparison_display_thread_params.redraw_request = 1;
    g_formation_equipment_panel_thread_params.redraw_request = 1;
    g_unit_panel_numeric_panel_params.redraw_request = 1;
    g_unit_panel_preview_stats_thread_params.redraw_request = 1;
    g_formation_change_banner_thread_params.redraw_request = 1;
    g_formation_stat_preview_panel_thread_params.redraw_request = 1;
    g_formation_ability_panel_thread_params.redraw_request = 1;
}

/* Ask the formation screen's panel threads to stop (both status banners, the
 * equipment/ability panel, the numeric panel and the two preview windows)
 * and clear the panel styles. */
void formation_stop_panels(void) {
    menu_request_thread_stop(THREAD_SCREEN_SLOT_UNIT_STATUS);
    menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
    menu_request_thread_stop(SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL);
    menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
    formation_toggle_preview_stats_window(0);
    formation_toggle_stat_preview_panel(0);
    g_unit_panel_preview_stats_thread_params.style = 0;
    g_formation_stat_preview_panel_thread_params.style = 0;
    g_formation_equipment_panel_thread_params.style = 0;
    g_formation_ability_panel_thread_params.style = 0;
}

/* Pick the formation unit's stat by the current stat-select mode; `value` is unset for any other mode. */
static s32 formation_get_unit_stat_by_mode(s32 unit_index) {
    s32 value;

    if (g_formation_stat_display_mode == MENU_UNIT_READOUT_HP) {
        value = g_formation_unit_pointers[unit_index]->status.gauges[MENU_UNIT_STATUS_GAUGE_HP].value;
    } else if (g_formation_stat_display_mode == MENU_UNIT_READOUT_MP) {
        value = g_formation_unit_pointers[unit_index]->status.gauges[MENU_UNIT_STATUS_GAUGE_MP].value;
    } else if (g_formation_stat_display_mode == MENU_UNIT_READOUT_CT) {
        value = MENU_NUMBER_VALUE_DASHES | 100;
    }
    return value;
}

/* Menu callback: return the formation unit's level. */
static s32 formation_get_unit_level(s32 unit_id) {
    return g_formation_unit_pointers[unit_id]->status.level;
}

/* Menu callback: return the formation unit's experience. */
static s32 formation_get_unit_experience(s32 unit_id) {
    return g_formation_unit_pointers[unit_id]->status.experience;
}

/* Menu callback: return the formation unit's Brave. */
static s32 formation_get_unit_brave(s32 unit_id) {
    return g_formation_unit_pointers[unit_id]->identity.brave;
}

/* Menu callback: return the formation unit's Faith. */
static s32 formation_get_unit_faith(s32 unit_id) {
    return g_formation_unit_pointers[unit_id]->identity.faith;
}

/* Menu callback: an egg's hatch progress, its total hatch time (high nibble of the birthday
 * byte) minus the days it still has to wait (low nibble). */
static s32 formation_get_egg_hatch_progress(s32 unit_id) {
    formation_unit_t* unit = g_formation_unit_pointers[unit_id];
    s32 low = unit->birthday & 0xF;

    return (unit->birthday >> 4) - low;
}

/* Menu callback: return an egg's total hatch time, kept in the high nibble of its birthday byte
 * (the low nibble counts down). */
static s32 formation_get_egg_hatch_total(s32 unit_id) {
    return g_formation_unit_pointers[unit_id]->birthday >> 4;
}

/* Returns 1 when the selected unit is in the two upper visible rows of the grid at
 * `scroll_position` (after the scroll in progress), which puts the unit banner at the bottom
 * of the screen. Returns 0 while browsing a unit that is not an egg. */
static s32 formation_is_selected_in_upper_rows(s32 scroll_position) {
    s32 result = 1;

    if (g_formation_unit_browse_enabled == 0
        || (g_formation_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG)) {
        if (g_formation_scroll_velocity < 0) {
            scroll_position -= 0x3c;
        }
        if (g_formation_scroll_velocity > 0) {
            scroll_position += 0x3c;
        }
        switch ((s8)scroll_position) {
        case -0x78:
            if (g_unit_selected_index >= 0x10) {
                result = 0;
            }
            break;
        case -0x3c:
            if (g_unit_selected_index >= 0xc) {
                result = 0;
            }
            break;
        case 0:
            if (g_unit_selected_index >= 8) {
                result = 0;
            }
            break;
        }
    } else {
        result = 0;
    }
    return result;
}

/* Step the unit grid's page scroll: at rest, start moving 4 pixels per frame toward the page that
 * holds the selected unit; while moving, accelerate by 4 and settle on the next 60-pixel page. */
static void formation_step_grid_scroll(s8* velocity, s8* position) {
    s8 next;

    if (*velocity == 0) {
        switch (*position) {
        case -0x78:
            if (g_unit_selected_index < 8) {
                *velocity = 4;
            }
            break;
        case -0x3C:
            if (g_unit_selected_index >= 0x10) {
                *velocity = -4;
            } else if (g_unit_selected_index < 4) {
                *velocity = 4;
            }
            break;
        case 0:
            if (g_unit_selected_index >= 0xC) {
                *velocity = -4;
            }
            break;
        }
    } else if (*velocity < 0) {
        next = *velocity - 4;
        *velocity = next;
        if (next < -0x3B) {
            *velocity = 0;
            *position -= 0x3C;
        }
    } else if (*velocity > 0) {
        next = *velocity + 4;
        *velocity = next;
        if (next >= 0x3C) {
            *velocity = 0;
            *position += 0x3C;
        }
    }
}

/* Returns 1 while the unit grid is scrolling, or when the selected unit lies outside the
 * three rows shown at the current page offset, so that the grid has to scroll. */
s32 formation_is_grid_scroll_needed(void) {
    s32 result = 0;

    if (g_formation_scroll_velocity != 0) {
        result = 1;
    } else {
        switch (g_formation_scroll_position) {
        case -0x78:
            if (g_unit_selected_index < 8) {
                result = 1;
            }
            break;
        case -0x3C:
            if (g_unit_selected_index >= 0x10) {
                result = 1;
            } else if (g_unit_selected_index < 4) {
                result = 1;
            }
            break;
        case 0:
            if (g_unit_selected_index >= 0xC) {
                result = 1;
            }
            break;
        }
    }
    return result;
}

/*
 * Draw an on-screen button from its sprite pieces and animate its press.
 *
 * g_menu_button_press_timers[index] counts press frames (until 6 / vsync mode); a pressed
 * button is drawn one pixel lower with the highlight CLUT. Pieces
 * g_menu_button_piece_starts[index]..g_menu_button_piece_starts[index + 1] are drawn at OT depth 50 upward.
 * Compiled at -O1 like the other formation/shop sprite drawers.
 */
void menu_draw_pressable_button(s16 index, s32 x, s16 y, s16 pressed, s16 disabled, u16 sound_effect_id) {
    oriented_quad_t sprite;
    menu_button_piece_t* piece;
    s32 speed = g_menu_event_speed;
    s32 i;
    s32 count;
    s16 piece_index;
    s16 w;
    s32 product;
    s32 h;
    u32 flip;
    u16 clut;

    if (speed == 0) {
        speed = 1;
    }
    if (disabled != 0) {
        g_menu_button_press_timers[index] = 0;
        clut = g_menu_cursor_mode1_foreground_clut;
    } else if (pressed != 0) {
        g_menu_button_press_timers[index] = 1;
        g_sound_queued_effect_id = sound_effect_id;
    } else if (g_menu_button_press_timers[index] != 0 && g_menu_button_press_timers[index] < 6 / speed) {
        g_menu_button_press_timers[index]++;
    } else {
        clut = g_menu_cursor_mode0_foreground_clut;
        g_menu_button_press_timers[index] = 0;
    }
    if (g_menu_button_press_timers[index] != 0) {
        y++;
        clut = g_menu_button_clut;
    }
    piece_index = g_menu_button_piece_starts[index];
    count = g_menu_button_piece_starts[index + 1] - piece_index;
    sprite.source.clut = clut;
    sprite.source.tpage = g_menu_cursor_texture_page;
    for (i = 0; i < count; i++) {
        piece = &g_screen_button_pieces[piece_index];
        sprite.position.x = x + piece->x_offset;
        sprite.position.y = y + piece->y_offset;
        product = g_screen_button_piece_scales[piece_index].x * piece->w;
        w = ((product < 0) ? -product : product) >> 12;
        sprite.w = w;
        h = g_screen_button_piece_scales[piece_index].y * piece->h;
        h = (h < 0) ? -h : h;
        sprite.h = h >> 12;
        sprite.source.u = piece->u;
        sprite.source.v = piece->v;
        sprite.source.w = piece->w;
        sprite.source.h = piece->h;
        if (g_screen_button_piece_scales[piece_index].x >= 0x1005) {
            sprite.w = w + 2;
        }
        flip = g_screen_button_piece_scales[piece_index].x < 0;
        if (g_screen_button_piece_scales[piece_index].y < 0) {
            flip += 2;
        }
        menu_gfx_enqueue_oriented_textured_quad(&sprite, 0, flip, 0, i + 50);
        piece_index++;
    }
}

/*
 * Initialize the selected formation unit's stat callbacks and cursor position.
 *
 * The scroll update must report 10 stationary iterations before the cursor
 * coordinates are finalized. The four-column layout uses signed coordinates.
 */
void formation_init_selection_cursor(s16 formation_index) {
    point16_t* cursor;
    s32 count;
    s16 x;

    menu_init_scrollable_list_core(0, 0, 0);
    g_screen_script_callbacks[0].value = formation_get_unit_stat_by_mode;
    g_screen_script_callbacks[1].value = formation_get_unit_stat_by_mode;
    g_screen_script_callbacks[2].value = formation_get_egg_hatch_progress;
    g_screen_script_callbacks[3].value = formation_get_egg_hatch_total;
    g_screen_script_callbacks[4].value = formation_get_unit_level;
    g_screen_script_callbacks[5].value = formation_get_unit_experience;
    g_screen_script_callbacks[6].value = formation_get_unit_brave;
    g_screen_script_callbacks[7].value = formation_get_unit_faith;
    g_formation_scroll_velocity = 0;
    g_formation_scroll_unread = 0;
    g_unit_selected_index = formation_index;
    count = 0;
    formation_stage_selected_unit();
    do {
        formation_step_grid_scroll(&g_formation_scroll_velocity, &g_formation_scroll_position);
        if (g_formation_scroll_velocity == 0) {
            count++;
        }
    } while (count < 10);
    g_formation_unit_banner_at_bottom = formation_is_selected_in_upper_rows(g_formation_scroll_position);
    x = formation_index % 4;
    cursor = &g_formation_cursor_position;
    cursor->x = x * 62 + 34;
    /* x is reused for the scroll offset, read as a byte and then sign-extended. */
    x = g_formation_scroll_position;
    cursor->y = x + (formation_index >> 2) * 60 + 61;
    g_formation_selection_cursor_ready = 1;
    formation_reset_slots_and_frame_tpages();
}

/* Place the grid cursor on a formation slot, show HP under the units, and enable the unit banner
 * at the top or bottom where it last was. */
void formation_init_grid_cursor(s32 formation_index) {
    formation_init_selection_cursor(formation_index);
    g_formation_stat_display_mode = MENU_UNIT_READOUT_HP;
    g_formation_unit_banner_at_bottom_latch = g_formation_unit_banner_at_bottom;
    if (g_formation_unit_banner_at_bottom != 0) {
        g_unit_panel_status_display_thread_params.y = 0x90;
    } else {
        g_unit_panel_status_display_thread_params.y = 0;
    }
    g_formation_unit_banner_enabled = 1;
    g_formation_saved_banner_enabled = 1;
}

/*
 * Formation screen unit grid: L2/R2 cycle the readout under the units, Circle
 * and Cross turn browsing on and off and Square toggles the banner; then step
 * the grid cursor, draw each visible unit slot and ease the cursor trail
 * toward the selected slot.
 *
 * `value`, `row` and `col` each carry several short-lived roles, as the
 * target's s1/s3/s5 do; separate variables change the call-saved register
 * assignment. The two unused locals reproduce the target's frame layout.
 */
s32 formation_update_grid(s16 background_y, s16 toggle_banner, s16 allow_browse, s16 input, s16 locked,
    s32 (*get_mode)(s32), s32 (*get_shade)(s32)) {
    oriented_quad_t unused_desc;
    oriented_quad_t pos;
    rgb24_t unused_rgb;
    rgb24_t rgb;
    s32 enabled;
    s16 y;
    s32 row;
    s32 value;
    s32 col;
    s32 i;
    s16 mode;
    u8 rgb_value;
    menu_sprite_cell_t* cells;
    menu_instruction_t* window;
    rgb24_t* color;
    formation_unit_t* unit;

    if (locked != 0) {
        input = 0;
    }
    if (get_mode == NULL && g_formation_unit_browse_enabled == 0) {
        if (input & PSX_PAD_L2) {
            g_formation_stat_display_mode = g_formation_stat_display_mode != MENU_UNIT_READOUT_HP
                ? g_formation_stat_display_mode - 1
                : MENU_UNIT_READOUT_BRAVE_FAITH;
            input &= ~PSX_PAD_R2;
            g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
        }
        if (input & PSX_PAD_R2) {
            g_formation_stat_display_mode = g_formation_stat_display_mode != MENU_UNIT_READOUT_BRAVE_FAITH
                ? g_formation_stat_display_mode + 1
                : MENU_UNIT_READOUT_HP;
            g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
        }
    }
    enabled = 1;
    if (g_formation_triangle_menu_open == 0 && g_formation_banner_slide_step == 0 && g_formation_scroll_velocity == 0
        && g_formation_triangle_menu_running == 0) {
        if (allow_browse != 0 && screen_input_get_lockout() == 0 && formation_is_grid_scroll_needed() == 0) {
            if (input & PSX_PAD_CIRCLE) {
                if (g_formation_unit_browse_enabled == 0) {
                    if (g_formation_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG) {
                        g_sound_queued_effect_id = SOUND_SFX_INVALID;
                    } else if (thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0) {
                        g_formation_unit_browse_enabled = 1;
                        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
                    }
                }
            } else if ((input & PSX_PAD_CROSS) && g_formation_unit_browse_enabled != 0) {
                g_sound_queued_effect_id = SOUND_SFX_CANCEL;
                enabled = 0;
                g_formation_unit_browse_enabled = 0;
                menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
                menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
            }
        }
        if (toggle_banner != 0 && thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0
            && thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) == 0 && formation_is_grid_scroll_needed() == 0
            && (g_menu_input_primary_repeat & PSX_PAD_SQUARE)) {
            g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
            g_formation_unit_banner_enabled = g_formation_unit_banner_enabled == 0;
        }
    }
    /* While browsing, the grid cursor and the shoulder buttons are frozen: keep only the face
     * buttons, Select and Start. */
    if (g_formation_unit_browse_enabled != 0) {
        input &= 0xff0;
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0 && g_screen_grid_id == MENU_GRID_HELP_NONE
        && screen_fade_get_state() == SCREEN_FADE_STATE_NONE) {
        g_unit_selected_index
            = menu_update_wrapped_grid_selection(4, 5, g_formation_unit_count - 1, g_unit_selected_index, input);
    }
    formation_step_grid_scroll(&g_formation_scroll_velocity, &g_formation_scroll_position);

    /* The readout bar beside the banner: the L2/R2 buttons and the current readout's labels. */
    if (g_formation_banner_slide_step == 0 && screen_input_get_lockout() == 0 && enabled != 0 && get_mode == NULL
        && g_formation_unit_banner_enabled != 0 && g_formation_unit_browse_enabled == 0
        && g_formation_selection_cursor_ready != 0 && g_formation_current_menu != FORMATION_MENU_DISMISS_UNIT) {
        value = 0xf;
        if (g_formation_unit_banner_at_bottom != 0) {
            window = &g_formation_stat_bar_window_top;
            cells = g_formation_label_tiles;
        } else {
            value = 0xdb;
            window = &g_formation_stat_bar_window_bottom;
            cells = g_formation_label_tiles_bottom;
        }
        row = 0;
        if (thread_is_running(THREAD_SCREEN_SLOT_MENU) != 0) {
            row = g_formation_current_menu == FORMATION_MENU_UNIT_GRID;
        }
        row |= g_menu_input_lock_mode != 0 || g_screen_grid_id != MENU_GRID_HELP_NONE;
        menu_draw_pressable_button(
            MENU_PRESSABLE_BUTTON_L2, 0x26, value, input & PSX_PAD_L2, row, SOUND_SFX_PAGE_SWITCH);
        menu_draw_pressable_button(
            MENU_PRESSABLE_BUTTON_R2, 0xc2, value, input & PSX_PAD_R2, row, SOUND_SFX_PAGE_SWITCH);
        menu_run_script_with_palette_mode(window, 0, 0);
        menu_gfx_enqueue_textured_quad_list(
            cells, 0, 0, g_menu_cursor_texture_page, g_screen_palette_clut_normal, 0xb, 8);
        if (g_formation_stat_display_mode < 3) {
            row = g_formation_stat_display_mode;
            col = 1;
        } else if (g_formation_stat_display_mode == MENU_UNIT_READOUT_LEVEL_EXP) {
            row = 3;
            col = 2;
        } else if (g_formation_stat_display_mode == MENU_UNIT_READOUT_BRAVE_FAITH) {
            row = 5;
            col = 3;
        }
        menu_gfx_enqueue_textured_quad_list(
            &cells[row], 0, 0, g_menu_cursor_texture_page, g_screen_alternate_number_clut_normal, 0xc, col);
    }
    if (toggle_banner != 0) {
        g_formation_unit_banner_at_bottom = formation_is_selected_in_upper_rows(g_formation_scroll_position);
    }
    pos.w = 0x38;
    pos.h = 0x30;
    y = 0x24 + g_formation_scroll_position + g_formation_scroll_velocity;
    gfx_set_otag_index(8);
    i = 0;
    color = screen_get_draw_color();
    g_formation_saved_sprite_color.r = color->r;
    g_formation_saved_sprite_color.g = color->g;
    g_formation_saved_sprite_color.b = color->b;
    /* Draw the visible unit slots row by row; rows the banner covers are dimmed. */
    for (row = 0; row < 5; row++, y += 0x3c) {
        value = 0;
        if (g_formation_unit_banner_enabled != 0 && g_unit_panel_status_display_thread_params.flags == 0) {
            if (g_formation_unit_banner_at_bottom != 0) {
                value = y > 0x8f;
            } else if (y < 0x47) {
                value = 1;
            }
        }
        screen_set_sprite_color(value ? &g_formation_dimmed_sprite_color : &g_formation_normal_sprite_color);
        for (col = 0; col < 4; col++, i++) {
            if (i >= g_formation_unit_count) {
                break;
            }
            pos.position.x = col * 62 + 6;
            pos.position.y = y;
            if (i == g_unit_selected_index) {
                g_formation_cursor_target.x = col * 62 + 0x22;
                g_formation_cursor_target.y = y + 0x19;
            }
            if (y < -0x3c || y >= 0xf0) {
                continue;
            }
            if (g_formation_unit_browse_enabled == 0) {
                if (g_formation_unit_pointers[i]->proposition_status & FORMATION_UNIT_PROPOSITION_LEAVING) {
                    mode = MENU_UNIT_READOUT_LEAVING;
                } else if (g_formation_unit_pointers[i]->gender_flags & UNIT_FLAG_EGG) {
                    mode = MENU_UNIT_READOUT_EGG;
                } else if (g_formation_unit_pointers[i]->proposition_status & FORMATION_UNIT_PROPOSITION_MISSING) {
                    mode = MENU_UNIT_READOUT_MISSING;
                } else if (get_mode != NULL) {
                    mode = get_mode(i);
                } else {
                    mode = g_formation_stat_display_mode;
                }
                formation_draw_unit_readout(i, mode, pos.position.x, pos.position.y, color, get_shade, 0, 8);
            }
            /* Units brighten toward the cursor; a unit away on a proposition stays dim. */
            value = 0x50;
            if (g_formation_unit_pointers[i]->proposition_status == 0) {
                if (get_shade != NULL) {
                    value = get_shade(i);
                } else {
                    value = formation_cursor_distance_falloff(pos.position.x, pos.position.y, 0xc8, 0x50);
                    if (value > 0x80) {
                        value = 0x80;
                    }
                }
            }
            rgb_value = value;
            rgb.b = rgb_value;
            rgb.g = rgb_value;
            rgb.r = rgb_value;
            unit = g_formation_unit_pointers[i];
            if (unit->gender_flags & UNIT_FLAG_EGG) {
                formation_draw_egg_sprite(&pos.position, unit->egg_color, &rgb, unit->birthday & 0xf, 6);
            } else {
                formation_draw_unit_sprite(i, &pos.position, &rgb, 6);
            }
            g_formation_unit_dot_sprite.position.x = pos.position.x;
            g_formation_unit_dot_sprite.position.y = pos.position.y;
            g_formation_unit_dot_sprite.source.tpage = GetTPage(0, 1, 0x3c0, 0x100);
            if (i == g_unit_selected_index) {
                /* The selected unit's dot pulses between -0x28 and 0x28 brighter. */
                g_formation_dot_glow_timer += g_formation_dot_glow_direction * 2;
                if (g_formation_dot_glow_timer > 0x28) {
                    g_formation_dot_glow_direction = -1;
                }
                if (g_formation_dot_glow_timer < -0x28) {
                    g_formation_dot_glow_direction = 1;
                }
                rgb.r += g_formation_dot_glow_timer;
                rgb.g += g_formation_dot_glow_timer;
                rgb.b += g_formation_dot_glow_timer;
            }
            menu_gfx_enqueue_oriented_textured_quad(&g_formation_unit_dot_sprite, &rgb, TEXTURE_UV_FLIP_NONE, 1, 8);
        }
    }
    screen_set_sprite_color(&g_formation_saved_sprite_color);
    formation_draw_cursor_trail(&g_formation_cursor_target);

    /* Ease the cursor a quarter of the way to its target, snapping once within 2 pixels. */
    g_formation_cursor_position.x = (g_formation_cursor_position.x * 3 + g_formation_cursor_target.x) / 4;
    g_formation_cursor_position.y = (g_formation_cursor_position.y * 3 + g_formation_cursor_target.y) / 4;
    row = g_formation_cursor_position.x - g_formation_cursor_target.x;
    if (row >= -2 && row <= 2) {
        g_formation_cursor_position.x = g_formation_cursor_target.x - 1;
    }
    row = g_formation_cursor_position.y - g_formation_cursor_target.y;
    if (row >= -2 && row <= 2) {
        g_formation_cursor_position.y = g_formation_cursor_target.y - 1;
    }
    formation_draw_background_tiles(g_formation_scroll_position + g_formation_scroll_velocity, background_y);
    return 1;
}

/* Distance-based falloff: `base` minus the (y-stretched) distance from the
 * formation cursor to (x, y), clamped so it never drops below `floor`.
 * `dx` also holds the result. */
static s32 formation_cursor_distance_falloff(s32 x, s32 y, s32 base, s32 floor) {
    s32 dx = abs(g_formation_cursor_position.x - x);
    s32 dy;

    dx *= dx;
    dy = abs(g_formation_cursor_position.y - y);
    dy *= 2;
    dy *= dy;
    dx = base - (csqrt((dy + dx) * ONE) >> 12);
    if (dx < floor) {
        dx = floor;
    }
    return dx;
}

/*
 * Draw the formation-screen background as 64x32 tiles from `y` downwards,
 * shading each tile corner by its distance from the formation cursor.
 *
 * The target reads the rectangle's y with `lhu` and sign-extends it for each
 * comparison, hence the `(s16)(u16)` reads. `draw_row` is bound to s4 because
 * the target recomputes `draw_row * 5` inside the tile loop; the loop
 * optimizer never treats a hard register as invariant, while a pseudo lets it
 * hoist that product and the shade-row addresses.
 */
void formation_draw_background_tiles(s32 y, s32 unused_1) {
    textured_rect_source_t rect;
    /* Unused local bytes and RGB alignment retain the target frame. */
    u8 unused_frame[8];
    rgb24_t rgb[4];
    u8 shades[16][5];
    s32 row;
    s32 col;
    register s32 draw_row __asm__("$20");
    s32 shade;
    formation_background_tile_t* tile;
    formation_background_tile_t(*tiles)[4];

    draw_row = 0;
    rect.y = y;
    rect.w = 0x40;
    rect.clut = 0x40;
    rect.h = 0x20;
    rect.tpage = 0x20;
    rect.v = 0xD8;
    for (row = 0; y < 0x101; row++) {
        for (col = 0; col < 5; col++) {
            shade = formation_cursor_distance_falloff(col << 6, y, 0xFF, 0x50);
            shades[row][col] = (shade >= 0xDD) ? 0xDC : shade;
        }
        y += 0x20;
    }
    rect.clut = g_screen_background_clut;
    rect.tpage = g_menu_cursor_texture_page;
    for (; rect.y < 0xF0; draw_row++) {
        tiles = g_formation_background_tiles;
        if (rect.y >= -0x20) {
            col = 0;
            tile = tiles[draw_row % 2];
            for (; col < 4; col++) {
                rect.x = tile[col].x;
                rect.u = tile[col].u;
                rgb[0].r = rgb[0].g = rgb[0].b = shades[draw_row][col];
                rgb[1].r = rgb[1].g = rgb[1].b = shades[draw_row][col + 1];
                rgb[2].r = rgb[2].g = rgb[2].b = shades[draw_row + 1][col];
                rgb[3].r = rgb[3].g = rgb[3].b = shades[draw_row + 1][col + 1];
                gfx_enqueue_gouraud_textured_quad(&rect, rgb, 0, rect.u == 0x58);
            }
        }
        rect.y += 0x20;
    }
}

/* Empty the eight cursor trail points (x = -1) and set the texture pages of
 * the cursor sprite and its shadow. */
static void formation_reset_slots_and_frame_tpages(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        g_formation_cursor_trail[i].x = -1;
    }
    g_formation_cursor_shadow_sprite.source.tpage = GetTPage(0, 2, 0x3c0, 0x100);
    g_formation_cursor_sprite.source.tpage = GetTPage(0, 1, 0x3c0, 0x100);
}

/*
 * Shift the formation cursor trail history, ease its newest point toward the
 * cursor, and draw the visible trail frames from newest to oldest.
 *
 * Each coordinate moves a quarter of the way per frame and snaps to the
 * target once the step stops changing it. A point is drawn only when its x+y
 * differs from the last drawn point by at least 5; older points are tinted
 * with the g_formation_cursor_trail_brightness brightness ramp. Each frame is drawn as four quadrant
 * pieces (menu_gfx_enqueue_oriented_textured_quad modes 0..3) from two sprite layers at OT 3 and 4.
 * Compiled at -O1 like the other formation sprite drawers; the mix of direct,
 * struct-pointer and s16-pointer accesses to the newest point selects the
 * target's address registers.
 */
static void formation_draw_cursor_trail(point16_t* cursor) {
    rgb24_t rgb;
    s32 i;
    s32 delta;
    s32 sum;
    s32 previous_sum;
    rgb24_t* color;
    s16* trail_y;
    point16_t* newest;

    for (i = 0; i < 7; i++) {
        g_formation_cursor_trail[i].x = g_formation_cursor_trail[i + 1].x;
        g_formation_cursor_trail[i].y = g_formation_cursor_trail[i + 1].y;
    }
    newest = &g_formation_cursor_trail[7];
    if (newest->x == -1) {
        g_formation_cursor_trail[7].x = cursor->x - 31;
        g_formation_cursor_trail[7].y = cursor->y - 1;
    } else {
        i = newest->x;
        newest->x = (i * 3 + (cursor->x - 31)) >> 2;
        delta = newest->x;
        if (delta < 0) {
            delta = -delta;
        }
        if (i == delta) {
            newest->x = cursor->x - 31;
        }
        trail_y = &g_formation_cursor_trail[7].y;
        i = *trail_y;
        *trail_y = (i * 3 + (cursor->y - 1)) >> 2;
        delta = *trail_y;
        if (delta < 0) {
            delta = -delta;
        }
        if (i == delta) {
            *trail_y = cursor->y - 1;
        }
    }
    previous_sum = 1000;
    for (i = 7; i >= 0; i--) {
        rgb.r = rgb.g = rgb.b = g_formation_cursor_trail_brightness[i];
        sum = g_formation_cursor_trail[i].x + g_formation_cursor_trail[i].y;
        delta = sum - previous_sum;
        if (delta < 0) {
            delta = -delta;
        }
        if (delta >= 5) {
            previous_sum = sum;
            color = 0;
            if (i != 7) {
                color = &rgb;
            }
            if (g_formation_cursor_trail[i].x != -1) {
                g_formation_cursor_sprite.position.x = g_formation_cursor_shadow_sprite.position.x
                    = g_formation_cursor_trail[i].x;
                g_formation_cursor_sprite.h = g_formation_cursor_sprite.source.h = g_formation_cursor_shadow_sprite.h
                    = g_formation_cursor_shadow_sprite.source.h = 16;
                g_formation_cursor_shadow_sprite.position.y = g_formation_cursor_trail[i].y;
                g_formation_cursor_sprite.position.y = g_formation_cursor_trail[i].y - 1;
                menu_gfx_enqueue_oriented_textured_quad(
                    &g_formation_cursor_shadow_sprite, color, TEXTURE_UV_FLIP_NONE, 1, 3);
                menu_gfx_enqueue_oriented_textured_quad(&g_formation_cursor_sprite, color, TEXTURE_UV_FLIP_NONE, 1, 4);
                g_formation_cursor_shadow_sprite.position.x += 31;
                g_formation_cursor_sprite.position.x += 31;
                menu_gfx_enqueue_oriented_textured_quad(
                    &g_formation_cursor_shadow_sprite, color, TEXTURE_UV_FLIP_X, 1, 3);
                menu_gfx_enqueue_oriented_textured_quad(&g_formation_cursor_sprite, color, TEXTURE_UV_FLIP_X, 1, 4);
                g_formation_cursor_sprite.position.x = g_formation_cursor_shadow_sprite.position.x
                    = g_formation_cursor_trail[i].x;
                g_formation_cursor_sprite.h = g_formation_cursor_sprite.source.h = g_formation_cursor_shadow_sprite.h
                    = g_formation_cursor_shadow_sprite.source.h = 15;
                g_formation_cursor_shadow_sprite.position.y = g_formation_cursor_trail[i].y + 16;
                g_formation_cursor_sprite.position.y = g_formation_cursor_trail[i].y + 15;
                menu_gfx_enqueue_oriented_textured_quad(
                    &g_formation_cursor_shadow_sprite, color, TEXTURE_UV_FLIP_Y, 1, 3);
                menu_gfx_enqueue_oriented_textured_quad(&g_formation_cursor_sprite, color, TEXTURE_UV_FLIP_Y, 1, 4);
                g_formation_cursor_shadow_sprite.position.x += 31;
                g_formation_cursor_sprite.position.x += 31;
                menu_gfx_enqueue_oriented_textured_quad(
                    &g_formation_cursor_shadow_sprite, color, TEXTURE_UV_FLIP_XY, 1, 3);
                menu_gfx_enqueue_oriented_textured_quad(&g_formation_cursor_sprite, color, TEXTURE_UV_FLIP_XY, 1, 4);
            }
        }
    }
}

/* Draw the formation units and the backdrop or egg decoration for each
 * visible slot. */
static void formation_draw_unit_slots(point16_t* positions, s32 shade) {
    rgb24_t sprite_rgb;
    rgb24_t backdrop_rgb;
    s32 i;
    s32 ot_index;
    s32 palette;
    formation_unit_t* unit;
    s32 brightness;
    s8 rgb_value;

    backdrop_rgb.b = shade;
    backdrop_rgb.g = shade;
    backdrop_rgb.r = shade;

    for (i = 0; i < g_formation_unit_count; i++) {
        if (positions[i].x < -0x1E || positions[i].y < -0x1E || positions[i].x >= 0x101 || positions[i].y >= 0xF1) {
            continue;
        }
        if (i != g_unit_selected_index) {
            if (g_formation_unit_pointers[i]->proposition_status & FORMATION_UNIT_PROPOSITION_LEAVING) {
                palette = MENU_UNIT_READOUT_LEAVING;
            } else {
                if (g_formation_unit_pointers[i]->gender_flags & UNIT_FLAG_EGG) {
                    palette = MENU_UNIT_READOUT_EGG;
                } else {
                    if (g_formation_unit_pointers[i]->proposition_status & FORMATION_UNIT_PROPOSITION_MISSING) {
                        palette = MENU_UNIT_READOUT_MISSING;
                    } else {
                        palette = g_formation_stat_display_mode;
                    }
                }
            }
            formation_draw_unit_readout(i, palette, positions[i].x, positions[i].y, &backdrop_rgb, 0, 0, 9);
            ot_index = 8;
        } else {
            ot_index = 10;
        }
        unit = g_formation_unit_pointers[i];
        if (unit->gender_flags & UNIT_FLAG_EGG) {
            formation_draw_egg_sprite(&positions[i], unit->egg_color, 0, unit->birthday & 0xF, ot_index);
        } else {
            brightness = formation_cursor_distance_falloff(positions[i].x, positions[i].y, 0xC8, 0x50);
            if (brightness > 0x80) {
                brightness = 0x80;
            }
            rgb_value = brightness;
            sprite_rgb.b = rgb_value;
            sprite_rgb.g = rgb_value;
            sprite_rgb.r = rgb_value;
            formation_draw_unit_sprite(i, &positions[i], &sprite_rgb, ot_index);
        }
    }
}

/* Steps the 24-frame formation-grid transition and draws it.
 *
 * The selected unit slides from its grid cell to the portrait rest point
 * (0x8b,0x9e; 0x61,0x63 when mode >= 2) and drags the cursor with it; the
 * other units drift sideways by frame^2/2 (alternating direction per row)
 * while the backdrop shade fades. Returns the frame counter, which wraps to
 * 0 and clears g_formation_unit_banner_at_bottom when the transition ends. Compiled at -O1. */
s32 formation_step_grid_transition(s32 mode) {
    point16_t positions[20];
    u8 frame;
    s32 spread;
    s32 shade;
    s32 i;
    s32 x;
    s32 y;
    s32 row_y;
    s32 target_x;
    s32 target_y;
    s32 direction;

    frame = g_formation_grid_transition_frame;
    spread = (frame * frame) >> 1;
    shade = 0x80 - (frame << 7) / 24;
    for (i = 0; i < g_formation_unit_count; i++) {
        row_y = (i / 4) * 60 + 0x24;
        y = g_formation_scroll_position + row_y;
        x = (i % 4) * 62 + 6;
        if (i == g_unit_selected_index) {
            if (mode < 2) {
                target_x = 0x8B;
                target_y = 0x9E;
            } else {
                target_x = 0x61;
                target_y = 0x63;
            }
            positions[i].x
                = (x * (24 - g_formation_grid_transition_frame) + target_x * g_formation_grid_transition_frame) / 24;
            positions[i].y
                = (y * (24 - g_formation_grid_transition_frame) + target_y * g_formation_grid_transition_frame) / 24;
            g_formation_cursor_position.x = positions[i].x + 0x1C;
            g_formation_cursor_position.y = positions[i].y + 0x19;
        } else {
            if (i < 4) {
                direction = -1;
            } else if (i < 8) {
                direction = 1;
            } else if (i < 12) {
                direction = -1;
            } else {
                direction = 1;
            }
            positions[i].x = x + spread * direction;
            positions[i].y = y;
        }
    }
    g_formation_grid_transition_frame++;
    if (g_formation_grid_transition_frame == 24) {
        g_formation_grid_transition_frame = 0;
        g_formation_unit_banner_at_bottom = 0;
    }
    formation_draw_unit_slots(positions, shade);
    return g_formation_grid_transition_frame;
}

/*
 * Step the 24-frame return from the formation portrait to the unit grid.
 *
 * This reverses formation_step_grid_transition: the frame counter runs
 * down from 24, sliding the selected unit from its rest point back to its
 * grid cell while the other rows close in. Returns 1 while animating and 0 on
 * the call after the last frame. The unused array reproduces the
 * original 16-byte gap below positions. Compiled at -O1.
 */
s32 formation_step_grid_return(s32 mode) {
    point16_t unused[4];
    point16_t positions[20];
    u8 frame;
    s32 spread;
    s32 shade;
    s32 i;
    s32 x;
    s32 y;
    s32 row_y;
    s32 target_x;
    s32 target_y;
    s32 direction;

    if (g_formation_grid_return_done != 0) {
        g_formation_grid_return_done = 0;
        return 0;
    }
    if (g_formation_grid_return_frame == 0) {
        formation_init_selection_cursor(g_unit_selected_index);
        g_formation_grid_return_done = 0;
        g_formation_grid_return_frame = 24;
    }
    frame = g_formation_grid_return_frame;
    spread = (frame * frame) >> 1;
    shade = 0x80 - ((frame - 1) << 7) / 24;
    for (i = 0; i < g_formation_unit_count; i++) {
        row_y = (i / 4) * 60 + 0x24;
        y = g_formation_scroll_position + row_y;
        x = (i % 4) * 62 + 6;
        if (i == g_unit_selected_index) {
            if (mode < 2) {
                target_x = 0x8B;
                target_y = 0x9E;
            } else {
                target_x = 0x61;
                target_y = 0x63;
            }
            positions[i].x = (x * (24 - g_formation_grid_return_frame) + target_x * g_formation_grid_return_frame) / 24;
            positions[i].y = (y * (24 - g_formation_grid_return_frame) + target_y * g_formation_grid_return_frame) / 24;
            g_formation_cursor_position.x = positions[i].x + 0x1C;
            g_formation_cursor_position.y = positions[i].y + 0x19;
        } else {
            if (i < 4) {
                direction = -1;
            } else if (i < 8) {
                direction = 1;
            } else if (i < 12) {
                direction = -1;
            } else {
                direction = 1;
            }
            positions[i].x = x + spread * direction;
            positions[i].y = y;
        }
    }
    g_formation_grid_return_frame--;
    if (g_formation_grid_return_frame == 0) {
        g_formation_grid_return_done = 1;
    }
    formation_draw_unit_slots(positions, shade);
    return 1;
}

/* Interpolates the sprite position from (0x61, 0x63) to (0x8b, 0x9e) over 24
 * steps and advances the step counter, wrapping back to zero. */
u8 formation_step_portrait_slide(void) {
    point16_t pos;
    s32 to_x = 0x8b;
    s32 to_y = 0x9e;
    s32 from_x = 0x61;
    s32 from_y = 0x63;
    s32 step;
    s32 remaining;

    step = g_formation_portrait_slide_frame;
    remaining = 24 - step;
    pos.x = (remaining * from_x + step * to_x) / 24;
    pos.y = (remaining * from_y + step * to_y) / 24;
    g_formation_cursor_position.x = pos.x + 0x1c;
    g_formation_cursor_position.y = pos.y + 0x19;
    formation_draw_unit_sprite(g_unit_selected_index, &pos, 0, 0x32);
    g_formation_portrait_slide_frame++;
    if (g_formation_portrait_slide_frame == 24) {
        g_formation_portrait_slide_frame = 0;
    }
    return g_formation_portrait_slide_frame;
}

/* Draw a formation unit's sprite at its resting position (0x8B, 0x9E). */
void formation_place_portrait_at_rest(s16 unit_index) {
    point16_t pos;

    pos.x = 0x8B;
    pos.y = 0x9E;
    formation_draw_unit_sprite(unit_index, &pos, 0, 6);
}

/* Draws one formation unit sprite centered on pos.
 *
 * When g_formation_dismiss_anim_timer selects this unit (high byte) the sprite is drawn shrunk by
 * (low byte + 1) without a shadow. Otherwise units with a proposition status
 * get a palette 9/11 backdrop first and the sprite is drawn with its shadow.
 * Compiled at -O1; the (s16) casts keep the centering offset from being
 * reassociated onto pos. */
void formation_draw_unit_sprite(s16 unit_index, point16_t* pos, rgb24_t* rgb, s32 ot_index) {
    oriented_quad_t sprite;
    s32 scale;
    s32 w;
    s32 h;
    rgb24_t* backdrop_rgb;

    formation_build_unit_graphic(unit_index, &sprite.source, 0);
    if (g_formation_dismiss_anim_timer != 0 && (g_formation_dismiss_anim_timer >> 8) == unit_index) {
        scale = (g_formation_dismiss_anim_timer & 0xFF) + 1;
        w = sprite.source.w / scale;
        h = sprite.source.h / scale;
        sprite.position.x = pos->x - (s16)(w / 2 - 0x1C);
        sprite.position.y = pos->y - (s16)(h / 2 - 0x18);
        sprite.w = w;
        sprite.h = h;
        menu_gfx_enqueue_oriented_textured_quad(&sprite, rgb, TEXTURE_UV_FLIP_NONE, 0, ot_index);
        return;
    }
    sprite.position.x = pos->x - (s16)(sprite.source.w / 2 - 0x1F);
    sprite.position.y = pos->y - (s16)(sprite.source.h / 2 - 0x18);
    sprite.w = sprite.source.w;
    sprite.h = sprite.source.h;
    if (g_formation_unit_pointers[unit_index]->proposition_status != 0) {
        if (unit_index == g_unit_selected_index) {
            backdrop_rgb = &g_formation_normal_sprite_color;
        } else {
            backdrop_rgb = screen_get_draw_color();
        }
        formation_draw_unit_readout(unit_index,
            g_formation_unit_pointers[unit_index]->proposition_status == FORMATION_UNIT_PROPOSITION_LEAVING
                ? MENU_UNIT_READOUT_LEAVING
                : MENU_UNIT_READOUT_MISSING,
            pos->x, pos->y, backdrop_rgb, 0, 1, ot_index + 2);
        rgb = &g_formation_away_unit_color;
    }
    formation_draw_sprite_with_shadow(&sprite, rgb, ot_index);
}

/*
 * Draw an egg unit's two layered egg sprites and their shadow at pos.
 *
 * Callers pass the unit's egg_color as palette and the low nibble of birthday
 * (hatch countdown; see formation_get_egg_hatch_progress) as
 * hatch_countdown; the second layer is drawn only while it is below 2.
 */
static void formation_draw_egg_sprite(
    point16_t* pos, s32 palette, const rgb24_t* rgb, s32 hatch_countdown, s32 ot_index) {
    g_formation_shadow_egg_sprites[2].position.x = g_formation_shadow_egg_sprites[1].position.x = pos->x + 0x14;
    g_formation_shadow_egg_sprites[2].position.y = g_formation_shadow_egg_sprites[1].position.y = pos->y + 0x10;
    g_formation_shadow_egg_sprites[2].source.clut = g_formation_shadow_egg_sprites[1].source.clut
        = GetClut((palette % 4) * 0x10 + 0x140, 0xFF);
    menu_gfx_enqueue_oriented_textured_quad(&g_formation_shadow_egg_sprites[1], rgb, TEXTURE_UV_FLIP_NONE, 0, ot_index);
    if (hatch_countdown < 2) {
        menu_gfx_enqueue_oriented_textured_quad(
            &g_formation_shadow_egg_sprites[2], rgb, TEXTURE_UV_FLIP_NONE, 0, ot_index + 1);
    }
    g_formation_shadow_egg_sprites[0].position.x = pos->x + 0x13;
    g_formation_shadow_egg_sprites[0].position.y = pos->y + 0x23;
    g_formation_shadow_egg_sprites[0].source.tpage = GetTPage(0, 2, 0x3C0, 0x100);
    menu_gfx_enqueue_oriented_textured_quad(
        &g_formation_shadow_egg_sprites[0], 0, TEXTURE_UV_FLIP_NONE, 1, ot_index - 1);
}

/* Draw a formation unit sprite and, one ordering-table entry behind it, its
 * semi-transparent ground shadow: under the feet of a normal sprite, or
 * lower and further right for a sprite wider than 24 texels. */
void formation_draw_sprite_with_shadow(const oriented_quad_t* pos, const rgb24_t* rgb, s32 ot_index) {
    menu_gfx_enqueue_oriented_textured_quad(pos, rgb, TEXTURE_UV_FLIP_NONE, 0, ot_index);
    if (pos->source.w >= 0x19) {
        g_formation_shadow_egg_sprites[0].position.x = pos->position.x + 0xC;
        g_formation_shadow_egg_sprites[0].position.y = pos->position.y + 0x23;
    } else {
        g_formation_shadow_egg_sprites[0].position.x = pos->position.x;
        g_formation_shadow_egg_sprites[0].position.y = pos->position.y + 0x1E;
    }
    g_formation_shadow_egg_sprites[0].source.tpage = GetTPage(0, 2, 0x3C0, 0x100);
    menu_gfx_enqueue_oriented_textured_quad(
        &g_formation_shadow_egg_sprites[0], 0, TEXTURE_UV_FLIP_NONE, 1, ot_index - 1);
}

/* Returns 1 when x and y both lie in 0..254: the number commands keep their position in bytes,
 * so readouts that would not fit are skipped. */
static s32 formation_coords_fit_in_byte(s32 x, s32 y) {
    if (x >= 0 && x < 0xff) {
        return y >= 0 && y < 0xff;
    }
    return 0;
}

/*
 * Draw the stat readout below a formation-screen unit sprite.
 *
 * Modes 0-6 draw the label tile, number commands and (layouts 0-2) the gauge
 * backdrop according to the formation layout g_formation_stat_display_mode; modes 8, 9 and 11
 * draw one oriented status sprite.
 *
 * The two do/while (0) blocks only add loop-depth weight to the references
 * inside them; global allocation then orders y before mode and priority
 * before color as the target's register assignment requires. top is reused
 * for y + 45 in layouts 3 and 4 so it shares s4 as in the target.
 */
static void formation_draw_unit_readout(
    s16 unit_index, s32 mode, s32 x, s32 y, rgb24_t* color, s32 (*get_value)(s32), s32 enabled, s32 priority) {
    RECT unused_rect;
    RECT rect;
    point16_t xy[10];
    s32 top;
    s16 label_y;
    s16 num_x;
    s16 num_y;
    u16 clut;
    rgb24_t* rgb;
    oriented_quad_t* sprite;

    if (mode < 0) {
        return;
    }
    do {
        gfx_set_otag_index(priority);
    } while (0);
    screen_set_sprite_color(color);
    if (mode < 7) {
        do {
            top = y + 40;
            if (top >= 240) {
                return;
            }
        } while (0);
        if (g_formation_stat_display_mode < 3) {
            /* HP, MP or CT: the label, the current and maximum values with a separator glyph between,
             * and the gauge. */
            rect.x = x;
            rect.y = top;
            rect.w = g_formation_label_tiles[mode].w;
            rect.h = g_formation_label_tiles[mode].h;
            menu_gfx_enqueue_textured_quad(&rect, g_formation_label_tiles[mode].u, g_formation_label_tiles[mode].v,
                color, 0, g_menu_cursor_texture_page, g_screen_alternate_number_clut, priority);
            num_x = x + 40;
            num_y = y + 45;
            if (formation_coords_fit_in_byte(num_x, num_y)) {
                g_formation_gauge_max_number.x = num_x;
                g_formation_gauge_max_number.y = num_y;
                g_formation_gauge_max_number.value = unit_index;
                menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_formation_gauge_max_number);
            }
            num_x = x + 20;
            num_y = top;
            if (formation_coords_fit_in_byte(num_x, num_y)) {
                g_formation_gauge_value_number.x = num_x;
                g_formation_gauge_value_number.y = num_y;
                g_formation_gauge_value_number.value = unit_index;
                menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_formation_gauge_value_number);
            }
            rect.x = x + 35;
            rect.y = top + 5;
            rect.w = 6;
            rect.h = 11;
            menu_gfx_enqueue_textured_quad(
                &rect, 0xB4, 0x10, color, 0, g_screen_window_tpage, g_screen_alternate_number_clut, priority);
            if (mode == MENU_UNIT_READOUT_HP) {
                clut = g_screen_hp_gauge_clut;
                rgb = g_formation_hp_gauge_colors;
            } else if (mode == MENU_UNIT_READOUT_MP) {
                clut = g_screen_mp_gauge_clut;
                rgb = g_formation_mp_gauge_colors;
            } else {
                clut = g_screen_ct_gauge_clut;
                rgb = g_formation_ct_gauge_colors;
            }
            rect.x = x;
            rect.y = top + 8;
            rect.w = 0x25;
            rect.h = 3;
            menu_gfx_enqueue_textured_quad(&rect, 0xD8, 0xCA, color, 0, g_menu_cursor_texture_page, clut, priority - 1);
            xy[0].x = g_formation_gauge_bar_offsets[0] + x;
            xy[0].y = g_formation_gauge_bar_offsets[1] + top + 8;
            xy[3].x = xy[1].x = 0x20;
            xy[1].x += xy[0].x;
            xy[1].y = g_formation_gauge_bar_offsets[1] + top + 8;
            xy[2].x = g_formation_gauge_bar_offsets[2] + x;
            xy[2].y = g_formation_gauge_bar_offsets[3] + top + 8;
            xy[3].x += xy[2].x;
            xy[3].y = g_formation_gauge_bar_offsets[3] + top + 8;
            gfx_enqueue_gouraud_quad(xy, rgb, 0, priority);
        } else if (g_formation_stat_display_mode == MENU_UNIT_READOUT_LEVEL_EXP) {
            s32 y40;

            top = y + 45;
            rect.x = x - 1;
            label_y = top;
            y40 = y + 40;
            rect.y = y40;
            rect.w = g_formation_label_tiles[3].w + 3;
            rect.h = g_formation_label_tiles[3].h + 1;
            menu_gfx_enqueue_textured_quad(&rect, g_formation_label_tiles[3].u, g_formation_label_tiles[3].v, color, 0,
                g_menu_cursor_texture_page, g_screen_alternate_number_clut, priority);
            rect.x = x + 26;
            rect.y = label_y;
            rect.w = g_formation_label_tiles[4].w + 2;
            rect.h = g_formation_label_tiles[4].h;
            menu_gfx_enqueue_textured_quad(&rect, g_formation_label_tiles[4].u, g_formation_label_tiles[4].v, color, 0,
                g_menu_cursor_texture_page, g_screen_alternate_number_clut, priority);
            num_x = x + 13;
            num_y = y40;
            if (formation_coords_fit_in_byte(num_x, num_y)) {
                g_formation_level_number.x = num_x;
                g_formation_level_number.y = num_y;
                g_formation_level_number.value = unit_index;
                menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_formation_level_number);
            }
            num_x = x + 45;
            num_y = top;
            if (formation_coords_fit_in_byte(num_x, num_y)) {
                g_formation_experience_number.x = num_x;
                g_formation_experience_number.y = num_y;
                g_formation_experience_number.value = unit_index;
                menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_formation_experience_number);
            }
        } else if (g_formation_stat_display_mode == MENU_UNIT_READOUT_BRAVE_FAITH) {
            s32 y40;

            top = y + 45;
            rect.x = x - 1;
            label_y = top;
            y40 = y + 40;
            rect.y = y40;
            rect.w = g_formation_label_tiles[5].w;
            rect.h = g_formation_label_tiles[5].h;
            menu_gfx_enqueue_textured_quad(&rect, g_formation_label_tiles[5].u, g_formation_label_tiles[5].v, color, 0,
                g_menu_cursor_texture_page, g_screen_alternate_number_clut, priority);
            rect.x += rect.w - 2;
            rect.y += 5;
            rect.w = 3;
            rect.h = 3;
            menu_gfx_enqueue_textured_quad(
                &rect, 0x3B, 0x15, color, 0, g_menu_cursor_texture_page, g_screen_alternate_number_clut, priority);
            rect.x = x + 26;
            rect.y = label_y;
            rect.w = g_formation_label_tiles[7].w;
            rect.h = g_formation_label_tiles[7].h;
            menu_gfx_enqueue_textured_quad(&rect, g_formation_label_tiles[7].u, g_formation_label_tiles[7].v, color, 0,
                g_menu_cursor_texture_page, g_screen_alternate_number_clut, priority);
            rect.x += rect.w;
            rect.y += 5;
            rect.w = 3;
            rect.h = 3;
            menu_gfx_enqueue_textured_quad(
                &rect, 0x3B, 0x15, color, 0, g_menu_cursor_texture_page, g_screen_alternate_number_clut, priority);
            num_x = x + 13;
            if (formation_coords_fit_in_byte(num_x - 3, (s16)y40)) {
                g_formation_brave_number.x = num_x;
                g_formation_brave_number_wide.y = g_formation_brave_number.y = y40;
                g_formation_brave_number_wide.value = g_formation_brave_number.value = unit_index;
                if (g_formation_unit_pointers[unit_index]->identity.brave >= 100) {
                    g_formation_brave_number_wide.x = num_x - 3;
                    menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_formation_brave_number_wide);
                } else {
                    menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_formation_brave_number);
                }
            }
            num_x = x + 45;
            num_y = top;
            if (formation_coords_fit_in_byte(num_x - 5, num_y)) {
                g_formation_faith_number.x = num_x;
                g_formation_faith_number_wide.y = g_formation_faith_number.y = num_y;
                g_formation_faith_number_wide.value = g_formation_faith_number.value = unit_index;
                if (g_formation_unit_pointers[unit_index]->identity.faith >= 100) {
                    g_formation_faith_number_wide.x = num_x - 5;
                    menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_formation_faith_number_wide);
                } else {
                    menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_formation_faith_number);
                }
            }
        }
        return;
    }
    if (mode == MENU_UNIT_READOUT_EQUIP_LABEL) {
        if (get_value(unit_index) < 100) {
            sprite = &g_formation_label_cant_equip;
            sprite->position.x = x + 9;
        } else {
            sprite = &g_formation_label_equipping;
            sprite->position.x = x + 11;
        }
    } else if (mode == MENU_UNIT_READOUT_LEAVING && enabled != 0) {
        sprite = &g_formation_label_leaving;
        sprite->position.x = x + 6;
    } else if (mode == MENU_UNIT_READOUT_MISSING && enabled != 0) {
        sprite = &g_formation_label_missing;
        sprite->position.x = x + 15;
    } else {
        return;
    }
    sprite->position.y = y + 40;
    sprite->source.tpage = GetTPage(0, 0, 0x180, 0);
    menu_gfx_enqueue_oriented_textured_quad(sprite, color, TEXTURE_UV_FLIP_NONE, 0, priority);
}
