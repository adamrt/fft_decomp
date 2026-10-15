#include "fft/world.h"
#include "psx/libgpu.h"

/*
 * Draw the stat readout below a formation-screen unit sprite.
 *
 * Modes 0-6 draw the label tile, number commands and (layouts 0-2) the gauge
 * backdrop according to the formation layout g_world_formation_stat_display_mode; modes 8, 9 and 11
 * draw one oriented status sprite.
 *
 * The two do/while (0) blocks only add loop-depth weight to the references
 * inside them; global allocation then orders y before mode and priority
 * before color as the target's register assignment requires. top is reused
 * for y + 45 in layouts 3 and 4 so it shares s4 as in the target.
 */
void world_formation_draw_graphics_below_sprite(
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
        world_menu_set_draw_priority(priority);
    } while (0);
    world_menu_set_sprite_color(color);
    if (mode < 7) {
        do {
            top = y + 40;
            if (top >= 240) {
                return;
            }
        } while (0);
        if (g_world_formation_stat_display_mode < 3) {
            rect.x = x;
            rect.y = top;
            rect.w = g_world_formation_label_tiles[mode].w;
            rect.h = g_world_formation_label_tiles[mode].h;
            menu_gfx_enqueue_textured_quad(&rect, g_world_formation_label_tiles[mode].u,
                g_world_formation_label_tiles[mode].v, color, 0, g_world_menu_icon_tpage,
                g_world_menu_alternate_number_clut, priority);
            num_x = x + 40;
            num_y = y + 45;
            if (world_coords_fit_in_byte(num_x, num_y)) {
                g_world_formation_gauge_max_number.x = num_x;
                g_world_formation_gauge_max_number.y = num_y;
                g_world_formation_gauge_max_number.value = unit_index;
                world_menu_script_draw_formatted_number((menu_instruction_t*)&g_world_formation_gauge_max_number);
            }
            num_x = x + 20;
            num_y = top;
            if (world_coords_fit_in_byte(num_x, num_y)) {
                g_world_formation_gauge_value_number.x = num_x;
                g_world_formation_gauge_value_number.y = num_y;
                g_world_formation_gauge_value_number.value = unit_index;
                world_menu_script_draw_formatted_number((menu_instruction_t*)&g_world_formation_gauge_value_number);
            }
            rect.x = x + 35;
            rect.y = top + 5;
            rect.w = 6;
            rect.h = 11;
            menu_gfx_enqueue_textured_quad(
                &rect, 0xB4, 0x10, color, 0, g_world_menu_window_tpage, g_world_menu_alternate_number_clut, priority);
            if (mode == MENU_UNIT_READOUT_HP) {
                clut = g_world_menu_hp_gauge_clut;
                rgb = g_world_formation_hp_gauge_colors;
            } else if (mode == MENU_UNIT_READOUT_MP) {
                clut = g_world_menu_mp_gauge_clut;
                rgb = g_world_formation_mp_gauge_colors;
            } else {
                clut = g_world_menu_ct_gauge_clut;
                rgb = g_world_formation_ct_gauge_colors;
            }
            rect.x = x;
            rect.y = top + 8;
            rect.w = 0x25;
            rect.h = 3;
            menu_gfx_enqueue_textured_quad(&rect, 0xD8, 0xCA, color, 0, g_world_menu_icon_tpage, clut, priority - 1);
            xy[0].x = g_world_formation_gauge_bar_offsets[0] + x;
            xy[0].y = g_world_formation_gauge_bar_offsets[1] + top + 8;
            xy[3].x = xy[1].x = 0x20;
            xy[1].x += xy[0].x;
            xy[1].y = g_world_formation_gauge_bar_offsets[1] + top + 8;
            xy[2].x = g_world_formation_gauge_bar_offsets[2] + x;
            xy[2].y = g_world_formation_gauge_bar_offsets[3] + top + 8;
            xy[3].x += xy[2].x;
            xy[3].y = g_world_formation_gauge_bar_offsets[3] + top + 8;
            gfx_enqueue_gouraud_quad(xy, rgb, 0, priority);
        } else if (g_world_formation_stat_display_mode == MENU_UNIT_READOUT_LEVEL_EXP) {
            s32 y40;

            top = y + 45;
            rect.x = x - 1;
            label_y = top;
            y40 = y + 40;
            rect.y = y40;
            rect.w = g_world_formation_label_tiles[3].w + 3;
            rect.h = g_world_formation_label_tiles[3].h + 1;
            menu_gfx_enqueue_textured_quad(&rect, g_world_formation_label_tiles[3].u,
                g_world_formation_label_tiles[3].v, color, 0, g_world_menu_icon_tpage,
                g_world_menu_alternate_number_clut, priority);
            rect.x = x + 26;
            rect.y = label_y;
            rect.w = g_world_formation_label_tiles[4].w + 2;
            rect.h = g_world_formation_label_tiles[4].h;
            menu_gfx_enqueue_textured_quad(&rect, g_world_formation_label_tiles[4].u,
                g_world_formation_label_tiles[4].v, color, 0, g_world_menu_icon_tpage,
                g_world_menu_alternate_number_clut, priority);
            num_x = x + 13;
            num_y = y40;
            if (world_coords_fit_in_byte(num_x, num_y)) {
                g_world_formation_level_number.x = num_x;
                g_world_formation_level_number.y = num_y;
                g_world_formation_level_number.value = unit_index;
                world_menu_script_draw_formatted_number((menu_instruction_t*)&g_world_formation_level_number);
            }
            num_x = x + 45;
            num_y = top;
            if (world_coords_fit_in_byte(num_x, num_y)) {
                g_world_formation_experience_number.x = num_x;
                g_world_formation_experience_number.y = num_y;
                g_world_formation_experience_number.value = unit_index;
                world_menu_script_draw_formatted_number((menu_instruction_t*)&g_world_formation_experience_number);
            }
        } else if (g_world_formation_stat_display_mode == MENU_UNIT_READOUT_BRAVE_FAITH) {
            s32 y40;

            top = y + 45;
            rect.x = x - 1;
            label_y = top;
            y40 = y + 40;
            rect.y = y40;
            rect.w = g_world_formation_label_tiles[5].w;
            rect.h = g_world_formation_label_tiles[5].h;
            menu_gfx_enqueue_textured_quad(&rect, g_world_formation_label_tiles[5].u,
                g_world_formation_label_tiles[5].v, color, 0, g_world_menu_icon_tpage,
                g_world_menu_alternate_number_clut, priority);
            rect.x += rect.w - 2;
            rect.y += 5;
            rect.w = 3;
            rect.h = 3;
            menu_gfx_enqueue_textured_quad(
                &rect, 0x3B, 0x15, color, 0, g_world_menu_icon_tpage, g_world_menu_alternate_number_clut, priority);
            rect.x = x + 26;
            rect.y = label_y;
            rect.w = g_world_formation_label_tiles[7].w;
            rect.h = g_world_formation_label_tiles[7].h;
            menu_gfx_enqueue_textured_quad(&rect, g_world_formation_label_tiles[7].u,
                g_world_formation_label_tiles[7].v, color, 0, g_world_menu_icon_tpage,
                g_world_menu_alternate_number_clut, priority);
            rect.x += rect.w;
            rect.y += 5;
            rect.w = 3;
            rect.h = 3;
            menu_gfx_enqueue_textured_quad(
                &rect, 0x3B, 0x15, color, 0, g_world_menu_icon_tpage, g_world_menu_alternate_number_clut, priority);
            num_x = x + 13;
            if (world_coords_fit_in_byte(num_x - 3, (s16)y40)) {
                g_world_formation_brave_number.x = num_x;
                g_world_formation_brave_number_wide.y = g_world_formation_brave_number.y = y40;
                g_world_formation_brave_number_wide.value = g_world_formation_brave_number.value = unit_index;
                if (g_world_unit_pointers[unit_index]->identity.brave >= 100) {
                    g_world_formation_brave_number_wide.x = num_x - 3;
                    world_menu_script_draw_formatted_number((menu_instruction_t*)&g_world_formation_brave_number_wide);
                } else {
                    world_menu_script_draw_formatted_number((menu_instruction_t*)&g_world_formation_brave_number);
                }
            }
            num_x = x + 45;
            num_y = top;
            if (world_coords_fit_in_byte(num_x - 5, num_y)) {
                g_world_formation_faith_number.x = num_x;
                g_world_formation_faith_number_wide.y = g_world_formation_faith_number.y = num_y;
                g_world_formation_faith_number_wide.value = g_world_formation_faith_number.value = unit_index;
                if (g_world_unit_pointers[unit_index]->identity.faith >= 100) {
                    g_world_formation_faith_number_wide.x = num_x - 5;
                    world_menu_script_draw_formatted_number((menu_instruction_t*)&g_world_formation_faith_number_wide);
                } else {
                    world_menu_script_draw_formatted_number((menu_instruction_t*)&g_world_formation_faith_number);
                }
            }
        }
        return;
    }
    if (mode == MENU_UNIT_READOUT_EQUIP_LABEL) {
        if (get_value(unit_index) < 100) {
            sprite = &g_world_formation_label_cant_equip;
            sprite->position.x = x + 9;
        } else {
            sprite = &g_world_formation_label_equipping;
            sprite->position.x = x + 11;
        }
    } else if (mode == MENU_UNIT_READOUT_LEAVING && enabled != 0) {
        sprite = &g_world_formation_label_leaving;
        sprite->position.x = x + 6;
    } else if (mode == MENU_UNIT_READOUT_MISSING && enabled != 0) {
        sprite = &g_world_formation_label_missing;
        sprite->position.x = x + 15;
    } else {
        return;
    }
    sprite->position.y = y + 40;
    sprite->source.tpage = GetTPage(0, 0, 0x180, 0);
    menu_gfx_enqueue_oriented_textured_quad(sprite, color, TEXTURE_UV_FLIP_NONE, 0, priority);
}
