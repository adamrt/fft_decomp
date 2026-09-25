#include "fft/menu.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Thread task 0x3b: the combined status panel for g_world_selected_unit_stat_detail. Lists the five
 * item names and the five ability labels in two text images, draws the item
 * icons, and redraws the double-buffered 0x3d8-byte frame each frame until the
 * thread's third parameter is set. The full relative of world_menu_equipment_panel_thread
 * (items) and world_menu_ability_panel_thread (abilities).
 *
 * `base` and `state` are never-modified locals: the target spills them and
 * rematerialises their addresses at each use. `mode` doubles as the ability
 * text id, which keeps both in one callee-saved register as in the target. */
void world_menu_equipment_and_ability_panel_thread(void) {
    world_status_thread_t* thread;
    world_status_frame_t* screen;
    world_status_frame_t* base;
    world_item_stat_detail_t* state;
    u8* cursor;
    s32 frame;
    s32 mode;
    s32 i;

    world_thread_set_current_task_id(NATIVE_THREAD_TASK_STATUS_PANEL);
    thread = *(world_status_thread_t**)((g_world_thread_current_id * NATIVE_THREAD_STRIDE) + (s32)g_world_threads);
    g_world_input_frame_controller_input = world_input_get_menu_controller(0);
    screen = g_world_change_banner_panel_frames;
    base = g_world_change_banner_panel_frames;
    state = &g_world_selected_unit_stat_detail;
    world_menu_build_line_box(&g_world_equipment_ability_panel_origin, &screen->menu);
    world_menu_init_column_frame_primitives((battle_menu_status_panel_menu_primitives_t*)screen);
    world_gfx_reset_record_texture_window_3(&screen->draw_area);
    for (i = 0; i < 2; i++) {
        screen->tiles[i].x0 += g_world_equipment_ability_panel_origin.x;
        screen->tiles[i].y0 += g_world_equipment_ability_panel_origin.y;
    }
    for (i = 0; i < 8; i++) {
        screen->lines[i].x0 += g_world_equipment_ability_panel_origin.x;
        screen->lines[i].y0 += g_world_equipment_ability_panel_origin.y;
        screen->lines[i].x1 += g_world_equipment_ability_panel_origin.x;
        screen->lines[i].y1 += g_world_equipment_ability_panel_origin.y;
    }
    world_menu_init_sprite_array(&screen->sprites[19], 5, 0x7cfc);
    world_menu_init_sprite_array(&screen->sprites[0], 12, 0x7d7c);
    world_menu_init_sprite_array(&screen->sprites[12], 7, 0x7c3c);
    screen->sprites[0].clut = 0x7cbc;
    screen->sprites[1].clut = 0x7cbc;
    world_script_copy_bytes(&screen[1], screen, sizeof(world_status_frame_t));

    frame = 0;
    for (;;) {
        screen = &base[frame & 1];
        mode = (state->two_hands != 0) * 2;
        if (state->generic_monster != 0) {
            mode++;
        }
        for (i = 0; i < 19; i++) {
            if (mode == 0) {
                world_gfx_init_image_loading((POLY_FT4*)&screen->sprites[i],
                    (const world_image_location_t*)g_world_editor_numeric_geometry,
                    (const world_image_location_t*)&g_world_equipment_panel_origin,
                    &g_world_equipment_panel_sprite_params_0[i]);
            }
            if (mode == 1) {
                world_gfx_init_image_loading((POLY_FT4*)&screen->sprites[i],
                    (const world_image_location_t*)g_world_editor_numeric_geometry,
                    (const world_image_location_t*)&g_world_equipment_panel_origin,
                    &g_world_equipment_panel_sprite_params_1[i]);
            }
            if (mode == 2) {
                world_gfx_init_image_loading((POLY_FT4*)&screen->sprites[i],
                    (const world_image_location_t*)g_world_editor_numeric_geometry,
                    (const world_image_location_t*)&g_world_equipment_panel_origin,
                    &g_world_equipment_panel_sprite_params_2[i]);
            }
            if (mode == 3) {
                world_gfx_init_image_loading((POLY_FT4*)&screen->sprites[i],
                    (const world_image_location_t*)g_world_editor_numeric_geometry,
                    (const world_image_location_t*)&g_world_equipment_panel_origin,
                    &g_world_equipment_panel_sprite_params_3[i]);
            }
        }
        if (state->generic_monster != 0) {
            for (i = 0; i < 5; i++) {
                u16 ability = state->ability_ids[i];
                if (ability == BATTLE_MENU_STATUS_PANEL_LABEL_NONE || ability == 0) {
                    screen->sprites[7 + i].x0 -= 0x200;
                }
            }
        }
        if (frame == 0 || thread->redraw_request == 1) {
            world_clear_menu_render_buffer(g_world_change_banner_text_image, 0xc80);
            g_world_menu_text_state.stride = 0x50;
            world_menu_set_text_origin(0, 0);
            for (i = 0; i < 5; i++) {
                if ((state->equipment[i] & 0xff) != 0xff) {
                    world_menu_display_text_entry((s16)state->equipment[i] + TEXT_ID_ITEM_NAME_BASE,
                        g_world_change_banner_text_image, &g_world_menu_text_state.origin_x);
                }
                g_world_menu_text_state.origin_y += 0x10;
            }
            LoadImage(&g_world_equipment_panel_item_text_rect, (u32*)g_world_change_banner_text_image);
            world_clear_menu_render_buffer(g_world_stat_preview_text_image, 0xc80);
            g_world_menu_text_state.stride = 0x50;
            world_menu_set_text_origin(0, 0);
            for (i = 0; i < 2; i++) {
                mode = state->ability_ids[i] + 0xb000;
                if (state->generic_monster != 0) {
                    mode = state->ability_ids[i] + TEXT_ID_ABILITY_NAME_BASE;
                }
                if ((u16)state->ability_ids[i] == BATTLE_MENU_STATUS_PANEL_LABEL_NONE) {
                    g_world_menu_text_state.origin_y += 0x10;
                } else {
                    world_menu_display_text_entry(
                        mode, g_world_stat_preview_text_image, &g_world_menu_text_state.origin_x);
                    g_world_menu_text_state.origin_y += 0x10;
                }
            }
            for (; i < 5; i++) {
                if (state->ability_ids[i] != -1) {
                    world_menu_display_text_entry(state->ability_ids[i] + TEXT_ID_ABILITY_NAME_BASE,
                        g_world_stat_preview_text_image, &g_world_menu_text_state.origin_x);
                }
                g_world_menu_text_state.origin_y += 0x10;
            }
            LoadImage(&g_world_equipment_panel_ability_text_rect, (u32*)g_world_stat_preview_text_image);
            thread->redraw_request = 0;
        }
        i = 0;
        cursor = (u8*)screen;
        for (; i < 5; i++) {
            world_gfx_init_image_loading((POLY_FT4*)&screen->sprites[19 + i],
                (const world_image_location_t*)g_world_panel_item_icon_texture,
                (const world_image_location_t*)&g_world_equipment_ability_panel_origin,
                &g_world_equipment_panel_icon_params[i]);
            if ((state->equipment[i] & 0xff) != 0xff) {
                world_build_item_icon_sprite(&screen->sprites[19 + i], (s16)state->equipment[i]);
            } else {
                ((world_status_frame_t*)cursor)->sprites[19].x0 = -0x200;
            }
            cursor += sizeof(SPRT);
        }
        g_world_gfx_draw_area_y = (u16)g_world_frame_arg != 0xf0 ? 0xf0 : 0;
        world_gfx_build_scaled_draw_area_pair_at_offset(&screen->draw_area, &g_world_equipment_ability_panel_draw_area,
            frame, (u16)g_world_frame_arg, (s32*)thread);
        screen->draw_offsets[0].x = thread->x - 0x80;
        screen->draw_offsets[0].y = thread->y + g_world_gfx_draw_area_y;
        SetDrawOffset(&screen->draw_offsets[0], &screen->draw_offsets[0].x);
        screen->draw_offsets[1].x = -0x80;
        screen->draw_offsets[1].y = g_world_gfx_draw_area_y;
        SetDrawOffset(&screen->draw_offsets[1], &screen->draw_offsets[1].x);
        world_menu_configure_status_panel_primitive_colors(
            (battle_menu_status_panel_primitives_t*)screen, (const battle_menu_status_panel_frame_config_t*)thread);
        world_menu_select_primitive_color_palette(&screen->menu, (world_menu_color_input_t*)thread);
        if (frame < 13) {
            world_gfx_draw_or_append_gpu_primitive(&screen->draw_area.areas[1]);
        }
        world_gfx_draw_or_append_gpu_primitive(&screen->draw_offsets[1]);
        for (i = 0; i < 12; i++) {
            world_gfx_draw_or_append_gpu_primitive(&screen->sprites[i]);
        }
        for (i = 14; i < 19; i++) {
            world_gfx_draw_or_append_gpu_primitive(&screen->sprites[i]);
        }
        world_gfx_draw_or_append_gpu_primitive(&screen->draw_modes[0]);
        world_gfx_draw_or_append_gpu_primitive(&screen->sprites[12]);
        world_gfx_draw_or_append_gpu_primitive(&screen->sprites[13]);
        world_gfx_draw_or_append_gpu_primitive(&screen->draw_modes[1]);
        for (i = 0; i < 5; i++) {
            world_gfx_draw_or_append_gpu_primitive(&screen->sprites[19 + i]);
        }
        world_gfx_draw_or_append_gpu_primitive(&screen->draw_modes[2]);
        world_gfx_draw_or_append_gpu_primitive(&screen->tiles[0]);
        world_gfx_draw_or_append_gpu_primitive(&screen->tiles[1]);
        for (i = 0; i < 8; i++) {
            world_gfx_draw_or_append_gpu_primitive(&screen->lines[i]);
        }
        world_gfx_submit_primitive_group((world_primitive_group_t*)&screen->menu);
        world_gfx_draw_or_append_gpu_primitive(&screen->draw_offsets[0]);
        if (frame < 13) {
            world_gfx_draw_or_append_gpu_primitive(&screen->draw_area);
        }
        world_thread_yield();
        if (world_thread_get_current_parameter_3() != 0) {
            break;
        }
        frame++;
    }
    world_thread_yield();
    world_thread_exit_current();
}
