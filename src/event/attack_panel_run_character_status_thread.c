#include "fft/event_attack.h"
#include "psx/gpu.h"
#include "psx/types.h"

#define FRAME_RECT g_attack_character_status_frame_rect

/* Thread task 0x3b: the combined status panel for the selected unit. REQUIRE
 * twin of bunit_panel_run_character_status_thread. */
void attack_panel_run_character_status_thread(void) {
    battle_menu_status_panel_frame_config_t* thread;
    battle_menu_status_panel_buffer_t* screen;
    battle_menu_status_panel_buffer_t* base;
    battle_menu_status_panel_slot_state_t* state;
    u8* cursor; /* Keep the screen base in a register; a SPRT* adds 0x1a0 before the loop. */
    s32 frame;
    s32 mode;
    s32 i;
    s32 scale;
    s32 mask;
    RECT* rect;
    RECT* offsets;

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_STATUS_PANEL);
    thread
        = (battle_menu_status_panel_frame_config_t*)g_battle_threads[g_battle_current_thread_id].function_parameter_1;
    g_attack_input_controller = battle_script_get_controller_input_pointer(0);
    screen = g_attack_panel_frames_a;
    base = g_attack_panel_frames_a;
    state = &g_attack_unit_editor_state.state;
    rect = FRAME_RECT;
    battle_menu_init_numeric_display_frame_primitives(rect, &screen->numeric_frame);
    attack_gfx_init_menu_tile_and_line_primitives(screen);
    attack_gfx_init_scaled_draw_area_packets(&screen->portrait);
    for (i = 0; i < 2; i++) {
        screen->tiles[i].x0 += rect->x;
        screen->tiles[i].y0 += rect->y;
    }
    i = 0;
    /* The line pass reloads the origin address instead of reusing `rect`. */
    offsets = FRAME_RECT;
    for (; i < 8; i++) {
        screen->lines[i].x0 += offsets->x;
        screen->lines[i].y0 += offsets->y;
        screen->lines[i].x1 += offsets->x;
        screen->lines[i].y1 += offsets->y;
    }
    battle_menu_init_sprite_array(&screen->sprites[19], 5, 0x7cfc);
    battle_menu_init_sprite_array(&screen->sprites[0], 12, 0x7d7c);
    battle_menu_init_sprite_array(&screen->sprites[12], 7, 0x7c3c);
    screen->sprites[0].clut = 0x7cbc;
    screen->sprites[1].clut = 0x7cbc;
    battle_copy_bytes(&screen[1], screen, sizeof(battle_menu_status_panel_buffer_t));

    frame = 0;
    for (;;) {
        screen = &base[frame & 1];
        mode = (state->two_hands != 0) * 2;
        if (state->generic_monster != 0) {
            mode++;
        }
        for (i = 0; i < 19; i++) {
            if (mode == 0) {
                battle_gfx_init_image_loading((POLY_FT4*)&screen->sprites[i], g_attack_editor_numeric_geometry,
                    g_attack_panel_origin_offsets, &g_attack_panel_label_layouts_mode0[i]);
            }
            if (mode == 1) {
                battle_gfx_init_image_loading((POLY_FT4*)&screen->sprites[i], g_attack_editor_numeric_geometry,
                    g_attack_panel_origin_offsets, &g_attack_panel_label_layouts_mode1[i]);
            }
            if (mode == 2) {
                battle_gfx_init_image_loading((POLY_FT4*)&screen->sprites[i], g_attack_editor_numeric_geometry,
                    g_attack_panel_origin_offsets, &g_attack_panel_label_layouts_mode2[i]);
            }
            if (mode == 3) {
                battle_gfx_init_image_loading((POLY_FT4*)&screen->sprites[i], g_attack_editor_numeric_geometry,
                    g_attack_panel_origin_offsets, &g_attack_panel_label_layouts_mode3[i]);
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
            battle_clear_menu_render_buffer(g_attack_panel_text_image_a, 0xc80);
            g_menu_inner_window_width = 0x50;
            battle_menu_set_text_origin(0, 0);
            for (i = 0; i < 5; i++) {
                if ((state->equipment[i] & 0xff) != 0xff) {
                    battle_menu_display_text_entry((s16)state->equipment[i] + TEXT_ID_ITEM_NAME_BASE,
                        g_attack_panel_text_image_a, &g_menu_text_state.origin_x);
                }
                g_menu_text_state.origin_y += 0x10;
            }
            LoadImage(&g_attack_panel_text_upload_rect_a, (u32*)g_attack_panel_text_image_a);
            battle_clear_menu_render_buffer(g_attack_panel_text_image_b, 0xc80);
            g_menu_inner_window_width = 0x50;
            battle_menu_set_text_origin(0, 0);
            for (i = 0; i < 2; i++) {
                mode = state->ability_ids[i] + 0xb000;
                if (state->generic_monster != 0) {
                    mode = state->ability_ids[i] + TEXT_ID_ABILITY_NAME_BASE;
                }
                if ((u16)state->ability_ids[i] == BATTLE_MENU_STATUS_PANEL_LABEL_NONE) {
                    g_menu_text_state.origin_y += 0x10;
                } else {
                    battle_menu_display_text_entry(mode, g_attack_panel_text_image_b, &g_menu_text_state.origin_x);
                    g_menu_text_state.origin_y += 0x10;
                }
            }
            for (; i < 5; i++) {
                if (state->ability_ids[i] != -1) {
                    battle_menu_display_text_entry(state->ability_ids[i] + TEXT_ID_ABILITY_NAME_BASE,
                        g_attack_panel_text_image_b, &g_menu_text_state.origin_x);
                }
                g_menu_text_state.origin_y += 0x10;
            }
            LoadImage(&g_attack_panel_text_upload_rect_b, (u32*)g_attack_panel_text_image_b);
            thread->redraw_request = 0;
        }
        i = 0;
        cursor = (u8*)screen;
        for (; i < 5; i++) {
            battle_gfx_init_image_loading((POLY_FT4*)&screen->sprites[19 + i], g_attack_panel_item_icon_texture,
                (const battle_image_location_t*)FRAME_RECT, &g_attack_panel_item_icon_layouts[i]);
            if ((state->equipment[i] & 0xff) != 0xff) {
                battle_get_item_graphic_data(&screen->sprites[19 + i], (s16)state->equipment[i]);
            } else {
                ((battle_menu_status_panel_buffer_t*)cursor)->sprites[19].x0 = -0x200;
            }
            cursor += sizeof(SPRT);
        }
        scale = g_main_gfx_screen_polarity * 0xf0;
        mask = scale ^ 0xf0;
        mask = -(mask != 0);
        g_attack_gfx_draw_offset_y = mask & 0xf0;
        attack_gfx_build_scaled_draw_area_packets(
            &screen->portrait, &g_attack_character_status_draw_area_rect, frame, scale, (const s16*)thread);
        screen->draw_offsets[0].x = thread->origin_x - 0x80;
        screen->draw_offsets[0].y = thread->origin_y + (u16)g_attack_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[0], &screen->draw_offsets[0].x);
        screen->draw_offsets[1].x = -0x80;
        screen->draw_offsets[1].y = g_attack_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[1], &screen->draw_offsets[1].x);
        attack_panel_set_primitive_colors(screen, thread);
        attack_gfx_apply_menu_palette_for_mode(&screen->numeric_frame, thread);
        if (frame < 13) {
            battle_gfx_draw_or_append_gpu_primitive(&screen->portrait.areas[1]);
        }
        battle_gfx_draw_or_append_gpu_primitive(&screen->draw_offsets[1]);
        for (i = 0; i < 12; i++) {
            battle_gfx_draw_or_append_gpu_primitive(&screen->sprites[i]);
        }
        for (i = 14; i < 19; i++) {
            battle_gfx_draw_or_append_gpu_primitive(&screen->sprites[i]);
        }
        battle_gfx_draw_or_append_gpu_primitive(&screen->draw_modes[0]);
        battle_gfx_draw_or_append_gpu_primitive(&screen->sprites[12]);
        battle_gfx_draw_or_append_gpu_primitive(&screen->sprites[13]);
        battle_gfx_draw_or_append_gpu_primitive(&screen->draw_modes[1]);
        for (i = 0; i < 5; i++) {
            battle_gfx_draw_or_append_gpu_primitive(&screen->sprites[19 + i]);
        }
        battle_gfx_draw_or_append_gpu_primitive(&screen->draw_modes[2]);
        battle_gfx_draw_or_append_gpu_primitive(&screen->tiles[0]);
        battle_gfx_draw_or_append_gpu_primitive(&screen->tiles[1]);
        for (i = 0; i < 8; i++) {
            battle_gfx_draw_or_append_gpu_primitive(&screen->lines[i]);
        }
        battle_menu_submit_numeric_display_frame_primitives(&screen->numeric_frame);
        battle_gfx_draw_or_append_gpu_primitive(&screen->draw_offsets[0]);
        if (frame < 13) {
            battle_gfx_draw_or_append_gpu_primitive(&screen->portrait);
        }
        battle_thread_yield();
        if (battle_thread_get_current_parameter_3() != 0) {
            break;
        }
        frame++;
    }
    battle_thread_yield();
    battle_thread_exit_current();
}
