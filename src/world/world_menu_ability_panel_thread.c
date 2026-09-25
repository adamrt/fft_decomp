/*
 * Thread task 0x3b: a two-column status panel that lists five ability labels
 * of an item stat-detail record. Thread 10 draws g_world_selected_unit_stat_detail into the first
 * frame pair, other threads draw g_world_item_preview_stat_detail into the
 * second. A trimmed relative of attack_panel_run_character_status_thread
 * (same 0x3d8-byte frame layout and callee sequence).
 */
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

void world_menu_ability_panel_thread(void) {
    s32 offset[2];
    /* Never referenced: the target frame has these 16 bytes between the
       offset pair and the spill slots of u/base/upload. */
    s32 unused[4];
    s32 u;
    world_status_frame_t* base;
    RECT* upload;
    world_status_thread_t* thread;
    u8* text;
    world_status_frame_t* screen;
    world_item_stat_detail_t* state;
    s32 frame;
    s32 i;
    s32 id;

    world_thread_set_current_task_id(NATIVE_THREAD_TASK_STATUS_PANEL);
    thread = *(world_status_thread_t**)((g_world_thread_current_id * NATIVE_THREAD_STRIDE) + (s32)g_world_threads);
    g_world_input_frame_controller_input = world_input_get_menu_controller(0);
    if (g_world_thread_current_id == 10) {
        text = g_world_change_banner_text_image;
        screen = g_world_change_banner_panel_frames;
        state = &g_world_selected_unit_stat_detail;
        base = g_world_change_banner_panel_frames;
        upload = &g_world_equipment_panel_item_text_rect;
        u = 0;
    } else {
        text = g_world_stat_preview_text_image;
        screen = g_world_stat_preview_panel_frames;
        state = &g_world_item_preview_stat_detail;
        upload = &g_world_equipment_panel_ability_text_rect;
        base = g_world_stat_preview_panel_frames;
        u = 0x50;
    }
    world_menu_build_line_box(&g_world_ability_panel_origin, &screen->menu);
    world_menu_init_column_frame_primitives((world_menu_column_primitives_t*)screen);
    world_gfx_reset_record_texture_window_3(&screen->draw_area);
    screen->tiles[1].x0 += g_world_equipment_panel_origin.x;
    screen->tiles[1].y0 += g_world_equipment_panel_origin.y;
    for (i = 4; i < 8; i++) {
        screen->lines[i].x0 += g_world_equipment_panel_origin.x;
        screen->lines[i].y0 += g_world_equipment_panel_origin.y;
        screen->lines[i].x1 += g_world_equipment_panel_origin.x;
        screen->lines[i].y1 += g_world_equipment_panel_origin.y;
    }
    world_menu_init_sprite_array(&screen->sprites[0], 12, 0x7d7c);
    world_menu_init_sprite_array(&screen->sprites[12], 7, 0x7c3c);
    screen->sprites[0].clut = 0x7cbc;
    screen->sprites[1].clut = 0x7cbc;
    world_script_copy_bytes(&screen[1], screen, sizeof(world_status_frame_t));

    frame = 0;
    for (;;) {
        screen = &base[frame & 1];
        if (state->generic_monster == 0) {
            for (i = 0; i < 19; i++) {
                world_gfx_init_image_loading((POLY_FT4*)&screen->sprites[i],
                    (const world_image_location_t*)g_world_editor_numeric_geometry,
                    (const world_image_location_t*)&g_world_equipment_panel_origin,
                    &g_world_equipment_panel_sprite_params_0[i]);
            }
        } else {
            for (i = 0; i < 19; i++) {
                world_gfx_init_image_loading((POLY_FT4*)&screen->sprites[i],
                    (const world_image_location_t*)g_world_editor_numeric_geometry,
                    (const world_image_location_t*)&g_world_equipment_panel_origin,
                    &g_world_equipment_panel_sprite_params_1[i]);
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
        /* No i = 0 here: the target's reset before this test is reorg
           hoisting the loop's own initialisation into a delay slot, and an
           explicit one lengthens i's live range enough to lose it $s1. */
        if (frame == 0 || thread->redraw_request == 1) {
            world_clear_menu_render_buffer(text, 0xc80);
            g_world_menu_text_state.stride = 0x50;
            world_menu_set_text_origin(0, 0);
            for (i = 0; i < 2; i++) {
                id = state->ability_ids[i] + 0xb000;
                if (state->generic_monster != 0) {
                    id = state->ability_ids[i] + 0x7000;
                }
                if ((u16)state->ability_ids[i] == BATTLE_MENU_STATUS_PANEL_LABEL_NONE) {
                    g_world_menu_text_state.origin_y += 0x10;
                } else {
                    world_menu_display_text_entry(id, text, &g_world_menu_text_state.origin_x);
                    g_world_menu_text_state.origin_y += 0x10;
                }
            }
            for (; i < 5; i++) {
                if ((u16)state->ability_ids[i] != BATTLE_MENU_STATUS_PANEL_LABEL_NONE) {
                    world_menu_display_text_entry(
                        state->ability_ids[i] + 0x7000, text, &g_world_menu_text_state.origin_x);
                }
                g_world_menu_text_state.origin_y += 0x10;
            }
            LoadImage(upload, (u32*)text);
            thread->redraw_request = 0;
        }
        screen->sprites[12].u0 = u;
        screen->sprites[13].u0 = u;
        screen->draw_offsets[0].x = thread->x - 0xf3;
        g_world_gfx_draw_area_y = (u16)g_world_frame_arg != 0xf0 ? 0xf0 : 0;
        screen->draw_offsets[0].y = thread->y + g_world_gfx_draw_area_y;
        SetDrawOffset(&screen->draw_offsets[0], &screen->draw_offsets[0].x);
        screen->draw_offsets[1].x = -0x80;
        screen->draw_offsets[1].y = g_world_gfx_draw_area_y;
        SetDrawOffset(&screen->draw_offsets[1], &screen->draw_offsets[1].x);
        offset[0] = thread->x - 0x73;
        offset[1] = thread->y;
        world_gfx_build_scaled_draw_area_pair_at_offset(
            &screen->draw_area, &g_world_ability_panel_draw_area, frame, (u16)g_world_frame_arg, offset);
        world_menu_configure_status_panel_primitive_colors(
            (world_menu_status_panel_primitives_t*)screen, (const world_menu_status_panel_frame_config_t*)thread);
        world_menu_select_primitive_color_palette(&screen->menu, (world_menu_color_input_t*)thread);
        world_gfx_draw_or_append_gpu_primitive((s32*)&screen->draw_area.areas[1]);
        world_gfx_draw_or_append_gpu_primitive((s32*)&screen->draw_offsets[1]);
        world_gfx_draw_or_append_gpu_primitive((s32*)&screen->sprites[1]);
        for (i = 7; i < 12; i++) {
            world_gfx_draw_or_append_gpu_primitive((s32*)&screen->sprites[i]);
        }
        world_gfx_draw_or_append_gpu_primitive((s32*)&screen->draw_modes[0]);
        world_gfx_draw_or_append_gpu_primitive((s32*)&screen->sprites[13]);
        world_gfx_draw_or_append_gpu_primitive((s32*)&screen->draw_modes[1]);
        world_gfx_draw_or_append_gpu_primitive((s32*)&screen->tiles[1]);
        for (i = 4; i < 8; i++) {
            world_gfx_draw_or_append_gpu_primitive((s32*)&screen->lines[i]);
        }
        world_gfx_submit_primitive_group((world_primitive_group_t*)&screen->menu);
        world_gfx_draw_or_append_gpu_primitive((s32*)&screen->draw_offsets[0]);
        world_gfx_draw_or_append_gpu_primitive((s32*)&screen->draw_area);
        world_thread_yield();
        if (world_thread_get_current_parameter_3() != 0) {
            break;
        }
        frame++;
    }
    world_thread_yield();
    world_thread_exit_current();
}
