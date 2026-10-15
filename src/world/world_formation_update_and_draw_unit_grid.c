#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

#ifndef NULL
#    define NULL 0
#endif

/*
 * Formation screen unit grid: handle the palette, browse and banner toggles,
 * step the grid cursor, then draw each visible unit slot and ease the cursor
 * trail toward the selected slot.
 *
 * `value`, `row` and `col` each carry several short-lived roles, as the
 * target's s1/s3/s5 do; separate variables change the call-saved register
 * assignment. The two unused locals reproduce the target's frame layout.
 */
s32 world_formation_update_and_draw_unit_grid(s16 background_y, s16 toggle_banner, s16 allow_browse, s16 input,
    s16 locked, s32 (*get_mode)(s32), s32 (*get_shade)(s32)) {
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
    world_unit_t* unit;

    if (locked != 0) {
        input = 0;
    }
    if (get_mode == NULL && g_world_formation_unit_browse_enabled == 0) {
        if (input & PSX_PAD_L2) {
            g_world_formation_stat_display_mode = g_world_formation_stat_display_mode != MENU_UNIT_READOUT_HP
                ? g_world_formation_stat_display_mode - 1
                : MENU_UNIT_READOUT_BRAVE_FAITH;
            input &= ~PSX_PAD_R2;
            g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
        }
        if (input & PSX_PAD_R2) {
            g_world_formation_stat_display_mode = g_world_formation_stat_display_mode != MENU_UNIT_READOUT_BRAVE_FAITH
                ? g_world_formation_stat_display_mode + 1
                : MENU_UNIT_READOUT_HP;
            g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
        }
    }
    enabled = 1;
    if (g_world_formation_triangle_menu_open == 0 && g_world_formation_banner_slide_step == 0
        && g_world_formation_scroll_velocity == 0 && g_world_formation_triangle_menu_running == 0) {
        if (allow_browse != 0 && world_input_get_lockout_timer() == 0
            && world_formation_is_unit_list_scroll_needed() == 0) {
            if (input & PSX_PAD_CIRCLE) {
                if (g_world_formation_unit_browse_enabled == 0) {
                    if (g_world_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG) {
                        g_sound_queued_effect_id = MAIN_SFX_INVALID;
                    } else if (thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0) {
                        g_world_formation_unit_browse_enabled = 1;
                        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
                    }
                }
            } else if ((input & PSX_PAD_CROSS) && g_world_formation_unit_browse_enabled != 0) {
                g_sound_queued_effect_id = MAIN_SFX_CANCEL;
                enabled = 0;
                g_world_formation_unit_browse_enabled = 0;
                menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
                menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
            }
        }
        if (toggle_banner != 0 && thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0
            && thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) == 0
            && world_formation_is_unit_list_scroll_needed() == 0 && (g_menu_input_primary_repeat & PSX_PAD_SQUARE)) {
            g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
            g_world_formation_unit_banner_enabled = g_world_formation_unit_banner_enabled == 0;
        }
    }
    if (g_world_formation_unit_browse_enabled != 0) {
        input &= 0xff0;
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0 && g_world_grid_menu_id == MENU_GRID_HELP_NONE
        && world_gfx_get_fade_state() == WORLD_FADE_STATE_NONE) {
        g_unit_selected_index
            = menu_update_wrapped_grid_selection(4, 5, g_world_formation_unit_count - 1, g_unit_selected_index, input);
    }
    world_formation_step_scroll_velocity(&g_world_formation_scroll_velocity, &g_world_formation_scroll_position);
    if (g_world_formation_banner_slide_step == 0 && world_input_get_lockout_timer() == 0 && enabled != 0
        && get_mode == NULL && g_world_formation_unit_banner_enabled != 0 && g_world_formation_unit_browse_enabled == 0
        && g_world_formation_selection_cursor_ready != 0
        && g_world_formation_current_menu != WORLD_FORMATION_MENU_DISMISS_UNIT) {
        value = 0xf;
        if (g_world_formation_unit_banner_at_bottom != 0) {
            window = &g_world_formation_stat_bar_window_top;
            cells = g_world_formation_label_tiles;
        } else {
            value = 0xdb;
            window = &g_world_formation_stat_bar_window_bottom;
            cells = g_world_formation_label_tiles_bottom;
        }
        row = 0;
        if (thread_is_running(THREAD_SCREEN_SLOT_MENU) != 0) {
            row = g_world_formation_current_menu == WORLD_FORMATION_MENU_UNIT_GRID;
        }
        row |= g_menu_input_lock_mode != 0 || g_world_grid_menu_id != MENU_GRID_HELP_NONE;
        world_menu_draw_pressable_button(
            MENU_PRESSABLE_BUTTON_L2, 0x26, value, input & PSX_PAD_L2, row, MAIN_SFX_PAGE_SWITCH);
        world_menu_draw_pressable_button(
            MENU_PRESSABLE_BUTTON_R2, 0xc2, value, input & PSX_PAD_R2, row, MAIN_SFX_PAGE_SWITCH);
        menu_run_script_with_palette_mode_variant(window, 0, 0);
        menu_gfx_enqueue_textured_quad_list(
            cells, 0, 0, g_menu_cursor_texture_page, g_world_menu_palette_clut_normal, 0xb, 8);
        if (g_world_formation_stat_display_mode < 3) {
            row = g_world_formation_stat_display_mode;
            col = 1;
        } else if (g_world_formation_stat_display_mode == MENU_UNIT_READOUT_LEVEL_EXP) {
            row = 3;
            col = 2;
        } else if (g_world_formation_stat_display_mode == MENU_UNIT_READOUT_BRAVE_FAITH) {
            row = 5;
            col = 3;
        }
        menu_gfx_enqueue_textured_quad_list(
            &cells[row], 0, 0, g_menu_cursor_texture_page, g_world_menu_alternate_number_clut_normal, 0xc, col);
    }
    if (toggle_banner != 0) {
        g_world_formation_unit_banner_at_bottom
            = world_formation_is_selected_unit_in_upper_rows(g_world_formation_scroll_position);
    }
    pos.w = 0x38;
    pos.h = 0x30;
    y = 0x24 + g_world_formation_scroll_position + g_world_formation_scroll_velocity;
    world_menu_set_draw_priority(8);
    i = 0;
    color = menu_gfx_get_draw_color_variant();
    g_world_formation_saved_sprite_color.r = color->r;
    g_world_formation_saved_sprite_color.g = color->g;
    g_world_formation_saved_sprite_color.b = color->b;
    for (row = 0; row < 5; row++, y += 0x3c) {
        value = 0;
        if (g_world_formation_unit_banner_enabled != 0 && g_menu_status_display_thread_params.flags == 0) {
            if (g_world_formation_unit_banner_at_bottom != 0) {
                value = y > 0x8f;
            } else if (y < 0x47) {
                value = 1;
            }
        }
        world_menu_set_sprite_color(
            value ? &g_world_formation_dimmed_sprite_color : &g_world_formation_normal_sprite_color);
        for (col = 0; col < 4; col++, i++) {
            if (i >= g_world_formation_unit_count) {
                break;
            }
            pos.position.x = col * 62 + 6;
            pos.position.y = y;
            if (i == g_unit_selected_index) {
                g_world_formation_cursor_target.x = col * 62 + 0x22;
                g_world_formation_cursor_target.y = y + 0x19;
            }
            if (y < -0x3c || y >= 0xf0) {
                continue;
            }
            if (g_world_formation_unit_browse_enabled == 0) {
                if (g_world_unit_pointers[i]->proposition_status & WORLD_FORMATION_UNIT_PROPOSITION_LEAVING) {
                    mode = MENU_UNIT_READOUT_LEAVING;
                } else if (g_world_unit_pointers[i]->gender_flags & UNIT_FLAG_EGG) {
                    mode = MENU_UNIT_READOUT_EGG;
                } else if (g_world_unit_pointers[i]->proposition_status & WORLD_FORMATION_UNIT_PROPOSITION_MISSING) {
                    mode = MENU_UNIT_READOUT_MISSING;
                } else if (get_mode != NULL) {
                    mode = get_mode(i);
                } else {
                    mode = g_world_formation_stat_display_mode;
                }
                world_formation_draw_graphics_below_sprite(
                    i, mode, pos.position.x, pos.position.y, color, get_shade, 0, 8);
            }
            value = 0x50;
            if (g_world_unit_pointers[i]->proposition_status == 0) {
                if (get_shade != NULL) {
                    value = get_shade(i);
                } else {
                    value = world_formation_cursor_distance_falloff(pos.position.x, pos.position.y, 0xc8, 0x50);
                    if (value > 0x80) {
                        value = 0x80;
                    }
                }
            }
            rgb_value = value;
            rgb.b = rgb_value;
            rgb.g = rgb_value;
            rgb.r = rgb_value;
            unit = g_world_unit_pointers[i];
            if (unit->gender_flags & UNIT_FLAG_EGG) {
                world_formation_draw_egg_sprite(&pos.position, unit->egg_color, &rgb, unit->birthday & 0xf, 6);
            } else {
                world_formation_draw_unit_sprite(i, &pos.position, &rgb, 6);
            }
            g_world_formation_unit_dot_sprite.position.x = pos.position.x;
            g_world_formation_unit_dot_sprite.position.y = pos.position.y;
            g_world_formation_unit_dot_sprite.source.tpage = GetTPage(0, 1, 0x3c0, 0x100);
            if (i == g_unit_selected_index) {
                g_world_formation_dot_glow_timer += g_world_formation_dot_glow_direction * 2;
                if (g_world_formation_dot_glow_timer > 0x28) {
                    g_world_formation_dot_glow_direction = -1;
                }
                if (g_world_formation_dot_glow_timer < -0x28) {
                    g_world_formation_dot_glow_direction = 1;
                }
                rgb.r += g_world_formation_dot_glow_timer;
                rgb.g += g_world_formation_dot_glow_timer;
                rgb.b += g_world_formation_dot_glow_timer;
            }
            menu_gfx_enqueue_oriented_textured_quad(
                &g_world_formation_unit_dot_sprite, &rgb, TEXTURE_UV_FLIP_NONE, 1, 8);
        }
    }
    world_menu_set_sprite_color(&g_world_formation_saved_sprite_color);
    world_formation_draw_cursor_trail(&g_world_formation_cursor_target);
    g_world_formation_cursor_position.x
        = (g_world_formation_cursor_position.x * 3 + g_world_formation_cursor_target.x) / 4;
    g_world_formation_cursor_position.y
        = (g_world_formation_cursor_position.y * 3 + g_world_formation_cursor_target.y) / 4;
    row = g_world_formation_cursor_position.x - g_world_formation_cursor_target.x;
    if (row >= -2 && row <= 2) {
        g_world_formation_cursor_position.x = g_world_formation_cursor_target.x - 1;
    }
    row = g_world_formation_cursor_position.y - g_world_formation_cursor_target.y;
    if (row >= -2 && row <= 2) {
        g_world_formation_cursor_position.y = g_world_formation_cursor_target.y - 1;
    }
    world_formation_draw_background_tiles(
        g_world_formation_scroll_position + g_world_formation_scroll_velocity, background_y);
    return 1;
}
