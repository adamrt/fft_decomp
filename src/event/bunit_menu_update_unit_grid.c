#include "fft/bunit.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Formation-screen unit grid for the unit list menu: handle the stat-mode,
 * browse and banner toggles, step the grid cursor, draw each visible unit
 * panel with its stat readout for g_bunit_unit_stat_mode (layouts 0-2 label,
 * two numbers and gauge; 3 and 4 two labelled numbers), then ease the cursor
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
 *   together with the two g_bunit_unit_selected_index tests; the unused
 *   RECTs reproduce the locals area.
 */
s32 bunit_menu_update_unit_grid(s16 allow_banner_toggle, s16 allow_browse, s16 show_stat_panel, s16 allow_stat_cycle,
    u16 stat_mode, u16 buttons, s16 locked) {
    RECT unused[3];
    RECT cell;
    RECT rect;
    u16 bar_points[8];
    RECT unused2;
    u8 saved_color[3];
    s32 row;
    s32 col;
    u16 input;
    u8* menu;
    bunit_textured_quad_descriptor_t* quads;
    s16 y;
    s32 unit;
    u8* color;
    s32 unit_index;
    s32 cell_y;
    s32 top;
    s32 upper_y4;
    s32 upper_y3;
    u32 mode;
    u16 width;
    u16 clut;
    u8* gradient;
    s16 lower_y4;
    u8 number_y;
    u8 number_y3;
    s16 label_y3;
    s16 label_y4;
    s16 cursor_x;
    s16* focus;
    s16 cursor_y;

    input = buttons;
    if (locked != 0) {
        input = 0;
    }
    if (allow_stat_cycle != 0) {
        if (g_bunit_unit_browse_enabled == 0) {
            if (input & PSX_PAD_L2) {
                g_bunit_unit_stat_mode = g_bunit_unit_stat_mode == 0 ? 4 : g_bunit_unit_stat_mode - 1;
                input &= ~2;
                g_bunit_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
            }
            if (input & PSX_PAD_R2) {
                g_bunit_unit_stat_mode = g_bunit_unit_stat_mode == 4 ? 0 : g_bunit_unit_stat_mode + 1;
                g_bunit_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
            }
        }
    }
    if (allow_stat_cycle == 0) {
        g_bunit_unit_stat_mode = stat_mode;
    }
    unit = 1;
    if (allow_browse != 0 && bunit_input_get_lock_timer() == 0 && battle_thread_is_running(7) == 0
        && !(g_bunit_status_display_flags & 0x60) && g_bunit_status_banner_slide_step == 0
        && g_bunit_unit_list_scroll_step == 0) {
        if (input & PSX_PAD_CIRCLE) {
            if (g_bunit_unit_browse_enabled == 0 && battle_thread_is_running(9) == 0) {
                g_bunit_unit_browse_enabled = 1;
                g_bunit_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            }
        } else if (input & PSX_PAD_CROSS) {
            if (g_bunit_unit_browse_enabled != 0) {
                unit = 0;
                g_bunit_sound_queued_effect_id = MAIN_SFX_CANCEL;
                g_bunit_unit_browse_enabled = 0;
            }
        }
    }
    if (g_bunit_unit_browse_enabled != 0) {
        input &= 0xFFF;
    }
    if (allow_banner_toggle != 0 && g_bunit_unit_browse_enabled == 0 && (input & PSX_PAD_SQUARE)
        && bunit_menu_is_unit_list_scroll_needed() == 0 && battle_thread_is_running(9) == 0
        && battle_thread_is_running(7) == 0 && !(g_bunit_status_display_flags & 0x60)) {
        g_bunit_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
        g_bunit_status_banner_enabled = g_bunit_status_banner_enabled == 0;
    }
    if (battle_thread_is_running(9) == 0 && g_bunit_help_screen_id == 0 && bunit_gfx_get_fade_state() == 0) {
        g_bunit_unit_grid_selected_cell = bunit_menu_update_wrapped_grid_selection(
            4, 5, (u8)g_bunit_unit_reorderable_count - 1, g_bunit_unit_grid_selected_cell, (s16)input);
        g_bunit_unit_selected_index = (u8)g_bunit_unit_reorderable_indices[g_bunit_unit_grid_selected_cell];
    }
    bunit_menu_update_unit_list_scroll(&g_bunit_unit_list_scroll_step, &g_bunit_unit_list_page_offset);
    if (g_bunit_unit_browse_enabled != 0) {
        if (input & PSX_PAD_L1) {
            g_bunit_unit_selected_index
                = g_bunit_unit_selected_index == 0 ? g_bunit_unit_list_count - 1 : g_bunit_unit_selected_index - 1;
        } else if (input & PSX_PAD_R1) {
            g_bunit_unit_selected_index
                = g_bunit_unit_selected_index != g_bunit_unit_list_count - 1 ? g_bunit_unit_selected_index + 1 : 0;
        }
        g_bunit_unit_grid_selected_cell = g_bunit_unit_list_index_map[g_bunit_unit_selected_index];
        bunit_menu_draw_pressable_button(0, 0xC, 0xE, input & PSX_PAD_L1,
            bunit_menu_is_active_or_transitioning() != 0 || g_bunit_help_screen_id != 0, 6);
        bunit_menu_draw_pressable_button(1, 0xE0, 0xE, input & PSX_PAD_R1,
            bunit_menu_is_active_or_transitioning() != 0 || g_bunit_help_screen_id != 0, 6);
    }
    if (g_bunit_status_banner_slide_step == 0 && bunit_input_get_lock_timer() == 0 && unit != 0 && show_stat_panel != 0
        && g_bunit_status_banner_enabled != 0 && g_bunit_unit_browse_enabled == 0) {
        top = 0xF;
        if (g_bunit_status_banner_at_bottom != 0) {
            menu = g_bunit_unit_grid_stat_bar_script_top;
            quads = g_bunit_unit_grid_label_tiles;
        } else {
            top = 0xDB;
            menu = g_bunit_unit_grid_stat_bar_script_bottom;
            quads = g_bunit_unit_grid_label_tiles_bottom;
        }
        bunit_menu_draw_pressable_button(4, 0x26, (s16)top, input & PSX_PAD_L2,
            bunit_menu_is_active_or_transitioning() != 0 || g_bunit_help_screen_id != 0, 6);
        bunit_menu_draw_pressable_button(5, 0xC2, (s16)top, input & PSX_PAD_R2,
            bunit_menu_is_active_or_transitioning() != 0 || g_bunit_help_screen_id != 0, 6);
        bunit_menu_dispatch_with_override((s32)menu, 0, 0);
        bunit_gfx_enqueue_textured_quad_list(
            quads, 0, 0, g_bunit_menu_icon_texture_page, g_bunit_text_clut_2_mode0, 0xB, 8);
        mode = g_bunit_unit_stat_mode;
        if (mode < 3) {
            row = mode;
            col = 1;
        } else if (mode == 3) {
            row = 3;
            col = 2;
        } else if (mode == 4) {
            row = 5;
            col = 3;
        }
        bunit_gfx_enqueue_textured_quad_list(
            quads + row, 0, 0, g_bunit_menu_icon_texture_page, g_bunit_text_clut_1_mode0, 0xC, col);
    }
    row = 0;
    g_bunit_status_banner_at_bottom = bunit_menu_is_unit_list_row_visible(g_bunit_unit_list_page_offset);
    y = 0x24 + g_bunit_unit_list_page_offset + g_bunit_unit_list_scroll_step;
    cell.w = 0x38;
    cell.h = 0x30;
    bunit_gfx_set_otag_index(8);
    unit = 0;
    color = bunit_gfx_get_draw_color();
    saved_color[0] = color[0];
    saved_color[1] = color[1];
    saved_color[2] = color[2];
    for (; row < 5; row++, y += 0x3C) {
        top = 0;
        if (g_bunit_status_banner_enabled != 0) {
            if (g_bunit_status_banner_at_bottom != 0) {
                top = y > 0x8F;
            } else if (y < 0x47) {
                top = 1;
            }
        }
        bunit_gfx_set_draw_color(top ? g_bunit_unit_grid_dimmed_color : g_bunit_unit_grid_normal_color);
        for (col = 0; col < 4; col++, unit++) {
            if (unit >= (u8)g_bunit_unit_reorderable_count)
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
            unit_index = (u8)g_bunit_unit_reorderable_indices[unit];
            top = cell_y + 40;
            if (g_bunit_unit_browse_enabled == 0 && top < 240) {
                if (g_bunit_unit_stat_mode < 3) {
                    number_y = top;
                    rect.x = cell.x;
                    rect.y = top;
                    rect.w = ((u8*)g_bunit_unit_grid_label_tiles)[g_bunit_unit_stat_mode * 6 + 2];
                    rect.h = ((u8*)g_bunit_unit_grid_label_tiles)[g_bunit_unit_stat_mode * 6 + 3];
                    bunit_gfx_enqueue_textured_quad(&rect,
                        ((u8*)g_bunit_unit_grid_label_tiles)[g_bunit_unit_stat_mode * 6 + 4],
                        ((u8*)g_bunit_unit_grid_label_tiles)[g_bunit_unit_stat_mode * 6 + 5], color, 0,
                        g_bunit_menu_icon_texture_page, g_bunit_text_metric_1, 8);
                    g_bunit_unit_grid_max_stat_number.x = cell.x + 40;
                    g_bunit_unit_grid_max_stat_number.y = cell_y + 45;
                    g_bunit_unit_grid_max_stat_number.value = unit_index;
                    bunit_menu_script_draw_formatted_number(&g_bunit_unit_grid_max_stat_number);
                    g_bunit_unit_grid_stat_number.x = cell.x + 20;
                    g_bunit_unit_grid_stat_number.y = number_y;
                    g_bunit_unit_grid_stat_number.value = unit_index;
                    bunit_menu_script_draw_formatted_number(&g_bunit_unit_grid_stat_number);
                    rect.x = cell.x + 35;
                    rect.y = top + 5;
                    rect.w = 6;
                    rect.h = 11;
                    bunit_gfx_enqueue_textured_quad(
                        &rect, 0xB4, 0x10, color, 0, g_bunit_text_digit_texture_page, g_bunit_text_metric_1, 8);
                    if (g_bunit_unit_stat_mode == 0) {
                        clut = g_bunit_unit_grid_hp_gauge_clut;
                        gradient = g_bunit_unit_grid_hp_gauge_colors;
                    } else if (g_bunit_unit_stat_mode == 1) {
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
                    bunit_gfx_enqueue_textured_quad(
                        &rect, 0xD8, 0xCA, color, 0, g_bunit_menu_icon_texture_page, clut, 7);
                    bar_points[0] = g_bunit_unit_grid_gauge_offsets[0] + cell.x;
                    bar_points[1] = g_bunit_unit_grid_gauge_offsets[1] + top + 8;
                    if (g_bunit_unit_max_stat_value != 0) {
                        width = (g_bunit_unit_current_stat_value << 5) / g_bunit_unit_max_stat_value;
                    } else {
                        width = 0;
                    }
                    bar_points[6] = bar_points[2] = width;
                    bar_points[2] += bar_points[0];
                    bar_points[3] = g_bunit_unit_grid_gauge_offsets[1] + top + 8;
                    bar_points[4] = g_bunit_unit_grid_gauge_offsets[2] + cell.x;
                    bar_points[5] = g_bunit_unit_grid_gauge_offsets[3] + top + 8;
                    bar_points[6] += bar_points[4];
                    bar_points[7] = g_bunit_unit_grid_gauge_offsets[3] + top + 8;
                    bunit_gfx_enqueue_gouraud_quad(bar_points, gradient, 0, 8);
                } else if (g_bunit_unit_stat_mode == 3) {
                    top = cell_y + 45;
                    rect.x = cell.x - 1;
                    label_y3 = top;
                    number_y3 = top;
                    upper_y3 = cell_y + 40;
                    rect.y = upper_y3;
                    rect.w = g_bunit_unit_grid_label_tiles[3].w + 3;
                    rect.h = g_bunit_unit_grid_label_tiles[3].h + 1;
                    bunit_gfx_enqueue_textured_quad(&rect, g_bunit_unit_grid_label_tiles[3].u,
                        g_bunit_unit_grid_label_tiles[3].v, color, 0, g_bunit_menu_icon_texture_page,
                        g_bunit_text_metric_1, 8);
                    rect.x = cell.x + 26;
                    rect.y = label_y3;
                    rect.w = g_bunit_unit_grid_label_tiles[4].w + 2;
                    rect.h = g_bunit_unit_grid_label_tiles[4].h;
                    bunit_gfx_enqueue_textured_quad(&rect, g_bunit_unit_grid_label_tiles[4].u,
                        g_bunit_unit_grid_label_tiles[4].v, color, 0, g_bunit_menu_icon_texture_page,
                        g_bunit_text_metric_1, 8);
                    g_bunit_unit_grid_level_number.x = cell.x + 13;
                    g_bunit_unit_grid_level_number.y = upper_y3;
                    g_bunit_unit_grid_level_number.value = unit_index;
                    bunit_menu_script_draw_formatted_number(&g_bunit_unit_grid_level_number);
                    g_bunit_unit_grid_experience_number.x = cell.x + 45;
                    g_bunit_unit_grid_experience_number.y = number_y3;
                    g_bunit_unit_grid_experience_number.value = unit_index;
                    bunit_menu_script_draw_formatted_number(&g_bunit_unit_grid_experience_number);
                } else if (g_bunit_unit_stat_mode == 4) {
                    top = cell_y + 45;
                    rect.x = cell.x - 1;
                    label_y4 = top;
                    upper_y4 = cell_y + 40;
                    rect.y = upper_y4;
                    rect.w = g_bunit_unit_grid_label_tiles[5].w;
                    rect.h = g_bunit_unit_grid_label_tiles[5].h;
                    bunit_gfx_enqueue_textured_quad(&rect, g_bunit_unit_grid_label_tiles[5].u,
                        g_bunit_unit_grid_label_tiles[5].v, color, 0, g_bunit_menu_icon_texture_page,
                        g_bunit_text_metric_1, 8);
                    rect.x += rect.w - 2;
                    rect.y += 5;
                    rect.w = 3;
                    rect.h = 3;
                    bunit_gfx_enqueue_textured_quad(
                        &rect, 0x3B, 0x15, color, 0, g_bunit_menu_icon_texture_page, g_bunit_text_metric_1, 8);
                    rect.x = cell.x + 26;
                    rect.y = label_y4;
                    rect.w = g_bunit_unit_grid_label_tiles[7].w;
                    rect.h = g_bunit_unit_grid_label_tiles[7].h;
                    bunit_gfx_enqueue_textured_quad(&rect, g_bunit_unit_grid_label_tiles[7].u,
                        g_bunit_unit_grid_label_tiles[7].v, color, 0, g_bunit_menu_icon_texture_page,
                        g_bunit_text_metric_1, 8);
                    rect.x += rect.w;
                    rect.y += 5;
                    rect.w = 3;
                    rect.h = 3;
                    bunit_gfx_enqueue_textured_quad(
                        &rect, 0x3B, 0x15, color, 0, g_bunit_menu_icon_texture_page, g_bunit_text_metric_1, 8);
                    g_bunit_unit_grid_brave_number.x = cell.x + 13;
                    g_bunit_unit_grid_brave_number_wide.y = g_bunit_unit_grid_brave_number.y = upper_y4;
                    g_bunit_unit_grid_brave_number_wide.value = g_bunit_unit_grid_brave_number.value = unit_index;
                    if (g_bunit_unit_data[unit_index]->brave >= 100) {
                        g_bunit_unit_grid_brave_number_wide.x = cell.x + 10;
                        bunit_menu_script_draw_formatted_number(&g_bunit_unit_grid_brave_number_wide);
                    } else
                        bunit_menu_script_draw_formatted_number(&g_bunit_unit_grid_brave_number);
                    lower_y4 = top;
                    g_bunit_unit_grid_faith_number.x = cell.x + 45;
                    g_bunit_unit_grid_faith_number_wide.y = g_bunit_unit_grid_faith_number.y = lower_y4;
                    g_bunit_unit_grid_faith_number_wide.value = g_bunit_unit_grid_faith_number.value = unit_index;
                    if (g_bunit_unit_data[unit_index]->faith >= 100) {
                        g_bunit_unit_grid_faith_number_wide.x = cell.x + 40;
                        bunit_menu_script_draw_formatted_number(&g_bunit_unit_grid_faith_number_wide);
                    } else
                        bunit_menu_script_draw_formatted_number(&g_bunit_unit_grid_faith_number);
                }
            }
            bunit_gfx_draw_unit_sprite_and_name_plate((u8)g_bunit_unit_reorderable_indices[unit], cell.x, cell.y);
            if (g_bunit_unit_data[(u8)g_bunit_unit_reorderable_indices[unit]]->formation_order_key & 0x80) {
                bunit_gfx_draw_unit_sprite_and_name_plate(
                    bunit_unit_find_index_by_roster_id(
                        (s16)(g_bunit_unit_data[(u8)g_bunit_unit_reorderable_indices[unit]]->formation_order_key
                            - 0x80)),
                    cell.x, cell.y);
            }
        }
    }
    bunit_gfx_set_draw_color(saved_color);
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
