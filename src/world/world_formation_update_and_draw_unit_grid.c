#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

#ifndef NULL
#    define NULL 0
#endif

/* The slot record passed to the unit sprite and egg portrait drawers: the
 * world_gfx_sprite_desc_t layout with the quad size at +0x04/+0x06. */
typedef struct world_formation_slot_sprite {
    s16 x;
    s16 y;
    s16 w;
    s16 h;
    u8 _unused_08[0x14 - 0x08];
} world_formation_slot_sprite_t;

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
    s16 locked, s32 (*get_mode)(), s32 (*get_shade)()) {
    world_gfx_sprite_desc_t unused_desc;
    world_formation_slot_sprite_t pos;
    u8 unused_rgb[3];
    u8 rgb[3];
    s32 enabled;
    s16 y;
    s32 row;
    s32 value;
    s32 col;
    s32 i;
    s16 mode;
    u8 rgb_value;
    battle_menu_sprite_cell_t* cells;
    world_menu_window_command_t* window;
    u8* color;
    world_formation_unit_t* unit;

    if (locked != 0) {
        input = 0;
    }
    if (get_mode == NULL && g_world_formation_unit_browse_enabled == 0) {
        if (input & PSX_PAD_L2) {
            g_world_formation_stat_display_mode
                = g_world_formation_stat_display_mode != 0 ? g_world_formation_stat_display_mode - 1 : 4;
            input &= ~2;
            g_world_menu_sound_effect_id = MAIN_SFX_PAGE_SWITCH;
        }
        if (input & PSX_PAD_R2) {
            g_world_formation_stat_display_mode
                = g_world_formation_stat_display_mode != 4 ? g_world_formation_stat_display_mode + 1 : 0;
            g_world_menu_sound_effect_id = MAIN_SFX_PAGE_SWITCH;
        }
    }
    enabled = 1;
    if (g_world_formation_triangle_menu_open == 0 && g_world_formation_banner_slide_step == 0
        && g_world_formation_scroll_velocity == 0 && g_world_formation_triangle_menu_running == 0) {
        if (allow_browse != 0 && world_input_get_lockout_timer() == 0 && world_formation_can_scroll_slots_back() == 0) {
            if (input & PSX_PAD_CIRCLE) {
                if (g_world_formation_unit_browse_enabled == 0) {
                    if (g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->gender_flags
                        & UNIT_FLAG_EGG) {
                        g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
                    } else if (world_thread_is_running(9) == 0) {
                        g_world_formation_unit_browse_enabled = 1;
                        g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
                    }
                }
            } else if ((input & PSX_PAD_CROSS) && g_world_formation_unit_browse_enabled != 0) {
                g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
                enabled = 0;
                g_world_formation_unit_browse_enabled = 0;
                world_menu_stop_unit_status_banner_thread(9);
                world_menu_stop_unit_status_banner_thread(0xc);
            }
        }
        if (toggle_banner != 0 && world_thread_is_running(9) == 0 && world_thread_is_running(7) == 0
            && world_formation_can_scroll_slots_back() == 0 && (g_world_input_primary_repeat & PSX_PAD_SQUARE)) {
            g_world_menu_sound_effect_id = MAIN_SFX_PAGE_SWITCH;
            g_world_formation_unit_banner_enabled = g_world_formation_unit_banner_enabled == 0;
        }
    }
    if (g_world_formation_unit_browse_enabled != 0) {
        input &= 0xff0;
    }
    if (world_thread_is_running(9) == 0 && g_world_grid_menu_id == 0 && world_gfx_get_fade_state() == 0) {
        g_world_formation_selected_unit_index = world_menu_step_grid_cursor(
            4, 5, g_world_formation_unit_count - 1, g_world_formation_selected_unit_index, input);
    }
    world_formation_step_scroll_velocity(&g_world_formation_scroll_velocity, &g_world_formation_scroll_position);
    if (g_world_formation_banner_slide_step == 0 && world_input_get_lockout_timer() == 0 && enabled != 0
        && get_mode == NULL && g_world_formation_unit_banner_enabled != 0 && g_world_formation_unit_browse_enabled == 0
        && g_world_formation_selection_cursor_ready != 0 && g_world_formation_current_menu != 4) {
        value = 0xf;
        if (g_world_formation_scroll_enabled != 0) {
            window = &g_world_formation_stat_bar_window_top;
            cells = g_world_formation_label_tiles;
        } else {
            value = 0xdb;
            window = &g_world_formation_stat_bar_window_bottom;
            cells = g_world_formation_label_tiles_bottom;
        }
        row = 0;
        if (world_thread_is_running(6) != 0) {
            row = g_world_formation_current_menu == 0;
        }
        row |= g_world_thread_task_active != 0 || g_world_grid_menu_id != 0;
        /* Called through an unprototyped view: the target passes pressed and disabled without narrowing them. */
        ((void (*)())world_menu_draw_pressable_button)(4, 0x26, (s16)value, input & PSX_PAD_L2, (s16)row, 6);
        ((void (*)())world_menu_draw_pressable_button)(5, 0xc2, (s16)value, input & PSX_PAD_R2, (s16)row, 6);
        world_menu_run_script_with_palette_mode(window, 0, 0);
        world_gfx_add_sprite_cell_quads(cells, 0, 0, g_world_menu_icon_tpage, g_world_menu_palette_clut_normal, 0xb, 8);
        if (g_world_formation_stat_display_mode < 3) {
            row = g_world_formation_stat_display_mode;
            col = 1;
        } else if (g_world_formation_stat_display_mode == 3) {
            row = 3;
            col = 2;
        } else if (g_world_formation_stat_display_mode == 4) {
            row = 5;
            col = 3;
        }
        world_gfx_add_sprite_cell_quads(
            &cells[row], 0, 0, g_world_menu_icon_tpage, g_world_menu_alternate_number_clut_normal, 0xc, col);
    }
    if (toggle_banner != 0) {
        g_world_formation_scroll_enabled = world_formation_can_scroll_slots(g_world_formation_scroll_position);
    }
    pos.w = 0x38;
    pos.h = 0x30;
    y = 0x24 + g_world_formation_scroll_position + g_world_formation_scroll_velocity;
    world_menu_set_draw_priority(8);
    i = 0;
    color = (u8*)world_menu_get_sprite_color();
    g_world_formation_saved_sprite_color[0] = color[0];
    g_world_formation_saved_sprite_color[1] = color[1];
    g_world_formation_saved_sprite_color[2] = color[2];
    for (row = 0; row < 5; row++, y += 0x3c) {
        value = 0;
        if (g_world_formation_unit_banner_enabled != 0 && g_world_status_display_thread_params.flags == 0) {
            if (g_world_formation_scroll_enabled != 0) {
                value = y > 0x8f;
            } else if (y < 0x47) {
                value = 1;
            }
        }
        world_menu_set_sprite_color(
            value ? (u8*)g_world_formation_dimmed_sprite_color : (u8*)g_world_formation_normal_sprite_color);
        for (col = 0; col < 4; col++, i++) {
            if (i >= g_world_formation_unit_count) {
                break;
            }
            pos.x = col * 62 + 6;
            pos.y = y;
            if (i == g_world_formation_selected_unit_index) {
                g_world_formation_cursor_target.x = col * 62 + 0x22;
                g_world_formation_cursor_target.y = y + 0x19;
            }
            if (y < -0x3c || y >= 0xf0) {
                continue;
            }
            if (g_world_formation_unit_browse_enabled == 0) {
                if (g_world_formation_unit_pointers[i]->proposition_status & 1) {
                    mode = 9;
                } else if (g_world_formation_unit_pointers[i]->gender_flags & UNIT_FLAG_EGG) {
                    mode = 10;
                } else if (g_world_formation_unit_pointers[i]->proposition_status & 2) {
                    mode = 11;
                } else if (get_mode != NULL) {
                    mode = get_mode(i);
                } else {
                    mode = g_world_formation_stat_display_mode;
                }
                world_formation_draw_graphics_below_sprite(i, mode, pos.x, pos.y, color, (s32 (*)(s16))get_shade, 0, 8);
            }
            value = 0x50;
            if (g_world_formation_unit_pointers[i]->proposition_status == 0) {
                if (get_shade != NULL) {
                    value = get_shade(i);
                } else {
                    value = world_formation_cursor_distance_falloff(pos.x, pos.y, 0xc8, 0x50);
                    if (value > 0x80) {
                        value = 0x80;
                    }
                }
            }
            rgb_value = value;
            rgb[2] = rgb_value;
            rgb[1] = rgb_value;
            rgb[0] = rgb_value;
            unit = g_world_formation_unit_pointers[i];
            if (unit->gender_flags & UNIT_FLAG_EGG) {
                world_formation_draw_unit_portrait_frame(
                    (world_gfx_sprite_desc_t*)&pos, unit->egg_color, (s32)rgb, unit->birthday & 0xf, 6);
            } else {
                world_formation_draw_unit_sprite(i, (world_menu_point_t*)&pos, (s8*)rgb, 6);
            }
            g_world_formation_unit_dot_sprite.x = pos.x;
            g_world_formation_unit_dot_sprite.y = pos.y;
            g_world_formation_unit_dot_sprite.tpage = GetTPage(0, 1, 0x3c0, 0x100);
            if (i == g_world_formation_selected_unit_index) {
                g_world_formation_dot_glow_timer += g_world_formation_dot_glow_direction * 2;
                if (g_world_formation_dot_glow_timer > 0x28) {
                    g_world_formation_dot_glow_direction = -1;
                }
                if (g_world_formation_dot_glow_timer < -0x28) {
                    g_world_formation_dot_glow_direction = 1;
                }
                rgb[0] += g_world_formation_dot_glow_timer;
                rgb[1] += g_world_formation_dot_glow_timer;
                rgb[2] += g_world_formation_dot_glow_timer;
            }
            world_gfx_enqueue_oriented_textured_quad(
                (world_oriented_quad_t*)&g_world_formation_unit_dot_sprite, rgb, 0, 1, 8);
        }
    }
    world_menu_set_sprite_color(g_world_formation_saved_sprite_color);
    world_formation_draw_cursor_trail(&g_world_formation_cursor_target.x);
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
    /* The target passes a second argument the one-parameter callee ignores. */
    ((void (*)())world_formation_draw_background_tiles)(
        g_world_formation_scroll_position + g_world_formation_scroll_velocity, background_y);
    return 1;
}
