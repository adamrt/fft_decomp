#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/equip.h"
#include "fft/main_gfx.h"
#include "fft/menu.h"
#include "fft/menu_types.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* The frame is status_panel_buffer_t (0x3d8 bytes, double buffered); the
 * WORLD twin world_menu_equipment_panel_thread uses the same layout. */

void battle_gfx_init_image_loading(POLY_FT4* primitive, const battle_image_location_t* base_load,
    const battle_image_location_t* base_screen, const world_gfx_image_load_parameters_t* parameters);

/* Equipment panel thread (task 0x3b): lists the five equipped item names and
 * draws their icons; EQUIP twin of WORLD world_menu_equipment_panel_thread.
 *
 * The sprite sheet layout (0-3) comes from the two mode flags; the text image
 * is rebuilt on the first frame or on request. The frame offset goes through
 * the `area` local: the target reads it back through that register inside the
 * line loop but folds the tile y read to the absolute address. */
void equip_panel_run_equipment_list_thread(void) {
    u8* text;
    s32 u;
    status_panel_buffer_t* base;
    status_panel_slot_state_t* state;
    RECT* upload;
    RECT* area;
    status_panel_frame_config_t* thread;
    status_panel_buffer_t* screen;
    s32 frame;
    s32 mode;
    s32 bank;
    s32 id;
    s32 i;

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_STATUS_PANEL);
    thread
        = *(status_panel_frame_config_t**)((g_battle_current_thread_id * NATIVE_THREAD_STRIDE) + (s32)g_battle_threads);
    g_equip_input_controller = battle_script_get_controller_input_pointer(0);
    if (g_battle_current_thread_id == 10) {
        text = g_equip_panel_text_image_a;
        screen = g_equip_panel_frames_a;
        state = (status_panel_slot_state_t*)g_equip_unit_editor_stats;
        base = g_equip_panel_frames_a;
        upload = (RECT*)g_equip_panel_text_upload_rect_a;
        u = 0;
    } else {
        text = g_equip_panel_text_image_b;
        screen = g_equip_panel_frames_b;
        state = (status_panel_slot_state_t*)&g_equip_item_preview_stat_detail;
        upload = (RECT*)g_equip_panel_text_upload_rect_b;
        base = g_equip_panel_frames_b;
        u = 0x50;
    }
    area = (RECT*)g_equip_panel_origin_offsets;
    battle_menu_init_numeric_display_frame_primitives(area, &screen->numeric_frame);
    equip_gfx_init_menu_tile_and_line_primitives((status_panel_menu_primitives_t*)screen);
    equip_gfx_init_scaled_draw_area_packets(&screen->portrait);
    screen->tiles[0].x0 += area->x;
    screen->tiles[0].y0 += area->y;
    for (i = 0; i < 4; i++) {
        screen->lines[i].x0 += area->x;
        screen->lines[i].y0 += area->y;
        screen->lines[i].x1 += area->x;
        screen->lines[i].y1 += area->y;
    }
    battle_menu_init_sprite_array(&screen->sprites[0], 12, 0x7d7c);
    battle_menu_init_sprite_array(&screen->sprites[12], 7, 0x7c3c);
    screen->sprites[0].clut = 0x7cbc;
    screen->sprites[1].clut = 0x7cbc;
    battle_copy_bytes(&screen[1], screen, sizeof(status_panel_buffer_t));

    frame = 0;
    for (;;) {
        screen = &base[frame & 1];
        mode = (state->two_hands != 0) * 2;
        if (state->generic_monster != 0) {
            mode++;
        }
        for (i = 0; i < 19; i++) {
            if (mode == 0) {
                battle_gfx_init_image_loading((POLY_FT4*)&screen->sprites[i],
                    (const battle_image_location_t*)g_equip_editor_numeric_geometry,
                    (const battle_image_location_t*)g_equip_panel_origin_offsets,
                    (const world_gfx_image_load_parameters_t*)g_equip_panel_label_layouts_mode0 + i);
            }
            if (mode == 1) {
                battle_gfx_init_image_loading((POLY_FT4*)&screen->sprites[i],
                    (const battle_image_location_t*)g_equip_editor_numeric_geometry,
                    (const battle_image_location_t*)g_equip_panel_origin_offsets,
                    (const world_gfx_image_load_parameters_t*)g_equip_panel_label_layouts_mode1 + i);
            }
            if (mode == 2) {
                battle_gfx_init_image_loading((POLY_FT4*)&screen->sprites[i],
                    (const battle_image_location_t*)g_equip_editor_numeric_geometry,
                    (const battle_image_location_t*)g_equip_panel_origin_offsets,
                    (const world_gfx_image_load_parameters_t*)g_equip_panel_label_layouts_mode2 + i);
            }
            if (mode == 3) {
                battle_gfx_init_image_loading((POLY_FT4*)&screen->sprites[i],
                    (const battle_image_location_t*)g_equip_editor_numeric_geometry,
                    (const battle_image_location_t*)g_equip_panel_origin_offsets,
                    (const world_gfx_image_load_parameters_t*)g_equip_panel_label_layouts_mode3 + i);
            }
        }
        if (frame == 0 || thread->redraw_request == 1) {
            battle_clear_menu_render_buffer(text, 0xc80);
            g_menu_inner_window_width = 0x50;
            battle_menu_set_text_origin(0, 0);
            for (i = 0; i < 5; i++) {
                if ((state->equipment[i] & 0xff) != 0xff) {
                    bank = state->equipment[i] & 0xc000;
                    g_menu_text_palette_offset = bank >> 12;
                    battle_menu_display_text_entry(
                        (state->equipment[i] & 0xff) | 0x3800, text, &g_menu_text_state.origin_x);
                }
                g_menu_text_state.origin_y += 0x10;
            }
            LoadImage(upload, (u32*)text);
            thread->redraw_request = 0;
        }
        screen->sprites[12].u0 = u;
        screen->sprites[13].u0 = u;
        battle_menu_init_sprite_array(&screen->sprites[19], 5, 0x7cfc);
        for (i = 0; i < 5; i++) {
            battle_gfx_init_image_loading((POLY_FT4*)&screen->sprites[19 + i],
                (const battle_image_location_t*)g_equip_panel_item_icon_texture,
                (const battle_image_location_t*)g_equip_panel_origin_offsets,
                (const world_gfx_image_load_parameters_t*)g_equip_panel_item_icon_layouts + i);
            id = state->equipment[i];
            if ((id & 0xff) != 0xff) {
                battle_get_item_graphic_data(&screen->sprites[19 + i], (s16)id);
            } else {
                (&screen->sprites[i])[19].x0 = 0;
            }
        }
        screen->sprites[12].u0 = u;
        screen->sprites[13].u0 = u;
        screen->draw_offsets[0].x = thread->origin_x - 0x80;
        g_equip_gfx_draw_offset_y = g_main_gfx_screen_polarity * 0xf0 != 0xf0 ? 0xf0 : 0;
        screen->draw_offsets[0].y = thread->origin_y + g_equip_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[0], &screen->draw_offsets[0].x);
        screen->draw_offsets[1].x = -0x80;
        screen->draw_offsets[1].y = g_equip_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[1], &screen->draw_offsets[1].x);
        equip_gfx_build_scaled_draw_area_packets(&screen->portrait, &g_equip_character_status_frame_rect[2], frame,
            g_main_gfx_screen_polarity * 0xf0, (const s16*)thread);
        equip_panel_set_primitive_colors((status_panel_primitives_t*)screen, thread);
        equip_gfx_apply_menu_palette_for_mode((s32)&screen->numeric_frame, (s32*)thread);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&screen->portrait.areas[1]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&screen->draw_offsets[1]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&screen->sprites[0]);
        for (i = 2; i < 7; i++) {
            battle_gfx_draw_or_append_gpu_primitive((s32*)&screen->sprites[i]);
        }
        for (i = 14; i < 19; i++) {
            battle_gfx_draw_or_append_gpu_primitive((s32*)&screen->sprites[i]);
        }
        battle_gfx_draw_or_append_gpu_primitive((s32*)&screen->draw_modes[0]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&screen->sprites[12]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&screen->draw_modes[1]);
        for (i = 0; i < 5; i++) {
            battle_gfx_draw_or_append_gpu_primitive((s32*)&screen->sprites[19 + i]);
        }
        battle_gfx_draw_or_append_gpu_primitive((s32*)&screen->draw_modes[2]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&screen->tiles[0]);
        for (i = 0; i < 4; i++) {
            battle_gfx_draw_or_append_gpu_primitive((s32*)&screen->lines[i]);
        }
        battle_menu_submit_numeric_display_frame_primitives((u8*)&screen->numeric_frame);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&screen->draw_offsets[0]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&screen->portrait);
        battle_thread_yield();
        if (battle_thread_get_current_parameter_3() != 0) {
            break;
        }
        frame++;
    }
    battle_thread_yield();
    battle_thread_exit_current();
}
