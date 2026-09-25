/*
 * WORLD twin: world_menu_run_unit_editor_panel_thread (same code, globals and callees rebound).
 */
#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/event.h"
#include "fft/main_gfx.h"
#include "fft/menu.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern world_unit_editor_frame_t g_battle_menu_unit_editor_frames[2];
extern u8 g_battle_unit_editor_line_colors[8 * WORLD_UNIT_EDITOR_LINE_SIZE];
extern world_unit_editor_panel_data_t g_battle_unit_editor_panel_data;

/* Unit debug editor panel thread (task 0x15). Builds two copies of the panel
 * frame (19 sprites, 5 item icons, two tiles, eight lines and the text images
 * for the unit's items and labels), then redraws the current copy each frame
 * until cancelled or told to stop; square opens the status list thread. The
 * WORLD twin is world_menu_run_unit_editor_panel_thread. */
void battle_menu_run_unit_editor_panel_thread(void) {
    /* The frame reserves 0x100 bytes for the first copy's line-box
       primitives, the stride of the palette records (WORLD 0x80195ed0). */
    union {
        world_menu_palette_primitives_t primitives;
        u8 storage[0x100];
    } palette;
    world_unit_editor_frame_t* records;
    world_unit_editor_frame_t* frame_record;
    LINE_F2* line;
    u8* buffer;
    /* One variable for the label text id and, in the frame loop, whether the
       status thread runs: the target keeps both in one callee-saved register. */
    s32 value;
    s32 frame;
    s32 i;

    battle_menu_open_debugchr_unit_editor();
    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_UNIT_EDITOR_PANEL);
    battle_menu_enter_status_screen_selection();
    g_option_menu_submenu_state = 1;
    g_battle_menu_panel_fade_mode = 2;
    battle_thread_set_parameters(0xA, 0, 0, 1);
    battle_thread_set_parameters(8, 0, 0, 1);
    battle_menu_record_selection_for_selected_unit();
    battle_menu_set_option_menu_open();
    do {
        battle_thread_yield();
    } while (g_battle_menu_hovered_stats_frame_count < 2);
    battle_menu_clear_option_menu_open();

    frame_record = g_battle_menu_unit_editor_frames;
    records = frame_record;
    battle_menu_set_disabled_texture_window(&frame_record->cursor_mode);
    g_battle_menu_hovered_unit_stats_display.y = 0x1A;
    g_battle_unit_summary_panel_rect.y = 0x1A;
    battle_gfx_set_draw_mode_for_texture_page(&frame_record->draw_modes[0], 0);
    battle_gfx_set_draw_mode_for_texture_page(&frame_record->draw_modes[1], 2);
    battle_gfx_set_draw_mode_for_texture_page(&frame_record->draw_modes[2], 4);
    SetTile(&frame_record->tiles[0]);
    SetSemiTrans(&frame_record->tiles[0], 1);
    frame_record->tiles[0].r0 = 0x30;
    frame_record->tiles[0].g0 = 0x30;
    frame_record->tiles[0].b0 = 0x30;
    frame_record->tiles[0].w = 0x10;
    frame_record->tiles[0].h = 0x5A;
    frame_record->tiles[0].x0 = g_battle_unit_editor_panel_rect.x;
    frame_record->tiles[0].y0 = g_battle_unit_editor_panel_rect.y + 1;
    battle_copy_bytes(&frame_record->tiles[1], &frame_record->tiles[0], sizeof(TILE));
    frame_record->tiles[0].x0 += 0x12;
    frame_record->tiles[1].x0 += 0x7C;
    line = frame_record->lines;
    for (i = 0; i < 8 * WORLD_UNIT_EDITOR_LINE_SIZE; i += WORLD_UNIT_EDITOR_LINE_SIZE) {
        SetLineF2(line);
        SetSemiTrans(line, 1);
        line->r0 = g_battle_unit_editor_line_colors[i + WORLD_UNIT_EDITOR_LINE_R];
        line->g0 = g_battle_unit_editor_line_colors[i + WORLD_UNIT_EDITOR_LINE_G];
        line->b0 = g_battle_unit_editor_line_colors[i + WORLD_UNIT_EDITOR_LINE_B];
        line->x0 = g_battle_unit_editor_line_colors[i + WORLD_UNIT_EDITOR_LINE_X0] + g_battle_unit_editor_panel_rect.x;
        line->y0 = g_battle_unit_editor_line_colors[i + WORLD_UNIT_EDITOR_LINE_Y0] + g_battle_unit_editor_panel_rect.y;
        line->x1 = g_battle_unit_editor_line_colors[i + WORLD_UNIT_EDITOR_LINE_X1] + g_battle_unit_editor_panel_rect.x;
        line->y1 = g_battle_unit_editor_line_colors[i + WORLD_UNIT_EDITOR_LINE_Y1] + g_battle_unit_editor_panel_rect.y;
        line++;
    }
    battle_menu_init_sprite_array(&frame_record->sprites[0], 0xC, 0x7D7C);
    battle_menu_init_sprite_array(&frame_record->sprites[12], 7, 0x7C3C);

    /* Each arm copies all four rows itself; the compiler cross-jumps the
       common tails, leaving only the first call's table loads in the arms. */
    if (g_battle_unit_editor_panel_data.layout != 0) {
        battle_copy_bytes(&g_battle_unit_editor_sprite_image_params_a[2], g_battle_unit_editor_row_image_params_b,
            2 * sizeof(world_gfx_image_load_parameters_t));
        battle_copy_bytes(&g_battle_unit_editor_sprite_image_params_a[14], &g_battle_unit_editor_row_image_params_b[2],
            2 * sizeof(world_gfx_image_load_parameters_t));
        battle_copy_bytes(&g_battle_unit_editor_sprite_image_params_b[2], g_battle_unit_editor_row_image_params_b,
            2 * sizeof(world_gfx_image_load_parameters_t));
        battle_copy_bytes(&g_battle_unit_editor_sprite_image_params_b[14], &g_battle_unit_editor_row_image_params_b[2],
            2 * sizeof(world_gfx_image_load_parameters_t));
    } else {
        battle_copy_bytes(&g_battle_unit_editor_sprite_image_params_a[2], g_battle_unit_editor_row_image_params_a,
            2 * sizeof(world_gfx_image_load_parameters_t));
        battle_copy_bytes(&g_battle_unit_editor_sprite_image_params_a[14], &g_battle_unit_editor_row_image_params_a[2],
            2 * sizeof(world_gfx_image_load_parameters_t));
        battle_copy_bytes(&g_battle_unit_editor_sprite_image_params_b[2], g_battle_unit_editor_row_image_params_a,
            2 * sizeof(world_gfx_image_load_parameters_t));
        battle_copy_bytes(&g_battle_unit_editor_sprite_image_params_b[14], &g_battle_unit_editor_row_image_params_a[2],
            2 * sizeof(world_gfx_image_load_parameters_t));
    }
    /* The panel passes SPRT and RECT records the POLY_FT4/battle_image_location_t prototype does not name. */
    if (g_battle_unit_editor_panel_data.flag == 0) {
        for (i = 0; i < 19; i++) {
            ((void (*)(void*, void*, void*, void*))battle_gfx_init_image_loading)(&frame_record->sprites[i],
                &g_battle_menu_texture_location, &g_battle_unit_editor_panel_rect,
                &g_battle_unit_editor_sprite_image_params_a[i]);
        }
    } else {
        for (i = 0; i < 19; i++) {
            ((void (*)(void*, void*, void*, void*))battle_gfx_init_image_loading)(&frame_record->sprites[i],
                &g_battle_menu_texture_location, &g_battle_unit_editor_panel_rect,
                &g_battle_unit_editor_sprite_image_params_b[i]);
        }
    }
    battle_menu_init_sprite_array(&frame_record->sprites[19], 5, 0x7CFC);
    for (i = 0; i < 5; i++) {
        ((void (*)(void*, void*, void*, void*))battle_gfx_init_image_loading)(&frame_record->sprites[19 + i],
            &g_battle_item_icon_image_location, &g_battle_unit_editor_panel_rect,
            &g_battle_unit_editor_item_icon_image_params[i]);
        battle_get_item_graphic_data(&frame_record->sprites[19 + i], g_battle_unit_editor_panel_data.item_ids[i]);
    }

    buffer = battle_menu_alloc_buffer(0xC80);
    battle_clear_menu_render_buffer(buffer, 0xC80);
    g_menu_text_state.stride = 0x50;
    battle_menu_set_text_origin(0, 0);
    for (i = 0; i < 5; i++) {
        value = g_battle_unit_editor_panel_data.item_ids[i] + TEXT_ID_ITEM_NAME_BASE;
        if ((u16)g_battle_unit_editor_panel_data.item_ids[i] == 0xFFFF) {
            g_menu_text_state.origin_y += 0x10;
        } else {
            battle_menu_display_text_entry(value, buffer, &g_menu_text_state.origin_x);
            g_menu_text_state.origin_y += 0x10;
        }
    }
    LoadImage(&g_battle_unit_editor_text_rect_a, (u32*)buffer);
    battle_thread_yield();
    battle_clear_menu_render_buffer(buffer, 0xC80);
    g_menu_text_state.stride = 0x50;
    battle_menu_set_text_origin(0, 0);
    for (i = 0; i < 2; i++) {
        value = g_battle_unit_editor_panel_data.label_text_ids[i] + 0xB000;
        if (g_battle_unit_editor_panel_data.flag != 0) {
            value = g_battle_unit_editor_panel_data.label_text_ids[i] + TEXT_ID_ABILITY_NAME_BASE;
        }
        if ((u16)g_battle_unit_editor_panel_data.label_text_ids[i] == 0xFFFF) {
            g_menu_text_state.origin_y += 0x10;
        } else {
            battle_menu_display_text_entry(value, buffer, &g_menu_text_state.origin_x);
            g_menu_text_state.origin_y += 0x10;
        }
    }
    for (i = 2; i < 5; i++) {
        value = g_battle_unit_editor_panel_data.label_text_ids[i] + TEXT_ID_ABILITY_NAME_BASE;
        if ((u16)g_battle_unit_editor_panel_data.label_text_ids[i] == 0xFFFF) {
            g_menu_text_state.origin_y += 0x10;
        } else {
            battle_menu_display_text_entry(value, buffer, &g_menu_text_state.origin_x);
            g_menu_text_state.origin_y += 0x10;
        }
    }
    if (g_battle_unit_editor_panel_data.flag != 0) {
        for (i = 0; i < 5; i++) {
            if ((u16)g_battle_unit_editor_panel_data.label_text_ids[i] == 0xFFFF) {
                frame_record->sprites[7 + i].x0 -= 0x200;
            }
        }
    }
    LoadImage(&g_battle_unit_editor_text_rect_b, (u32*)buffer);
    battle_thread_yield();
    battle_menu_free_memory(buffer);

    battle_copy_bytes(&records[1], frame_record, sizeof(world_unit_editor_frame_t));
    records[0].palette = &palette.primitives;
    records[1].palette = &g_battle_unit_editor_palette;
    battle_menu_init_numeric_display_frame_primitives(&g_battle_unit_editor_panel_rect, records[0].palette);
    battle_menu_init_numeric_display_frame_primitives(&g_battle_unit_editor_panel_rect, records[1].palette);
    battle_thread_start(0xA, battle_menu_run_numeric_display_panel_thread);
    battle_thread_set_parameters(0xA, 0, 0, 0);
    battle_thread_yield();

    frame = 0;
    for (;;) {
        battle_thread_yield();
        frame_record = &records[frame & 1];
        battle_menu_build_zoom_draw_area_pair(
            &frame_record->cursor_mode, &g_battle_unit_editor_cursor_rect, frame, g_main_gfx_screen_polarity);
        if (g_event_mode == 0 && g_battle_menu_help_open == 0 && battle_thread_get_current_parameter_3() != 0) {
            break;
        }
        value = battle_thread_is_running_8014cc94(g_battle_current_thread_id - 1);
        for (i = 0; i < 19; i++) {
            frame_record->sprites[i].clut = g_battle_unit_editor_sprite_clut_pairs[i][value];
        }
        for (i = 0; i < 5; i++) {
            if (value != 0) {
                SetShadeTex(&frame_record->sprites[19 + i], 0);
                /* Spelled from sprites[i]: the target's store pointer walks
                   frame_record + i * 0x14 with the item-icon bias in the
                   displacement; sprites[19 + i] adds the base per store. */
                (&frame_record->sprites[i])[19].r0 = 0x40;
                (&frame_record->sprites[i])[19].g0 = 0x40;
                (&frame_record->sprites[i])[19].b0 = 0x80;
            } else {
                SetShadeTex(&frame_record->sprites[19 + i], 1);
            }
        }
        if (value == 0) {
            battle_menu_init_primitive_colors_palette_bank_0(frame_record->palette);
        } else {
            battle_menu_init_primitive_colors_palette_bank_1(frame_record->palette);
        }
        if (battle_menu_can_accept_input() != 0) {
            if (g_battle_script_event_input & PSX_PAD_CROSS) {
                battle_thread_set_parameters(0xA, 0, 0, 1);
                break;
            }
            if ((g_battle_script_event_input & PSX_PAD_CIRCLE) && battle_thread_is_running_8014cc94(3) == 0
                && (battle_menu_has_status_effect_for_status_window(g_battle_active_turn_unit.battle_id) != 0
                    || (g_battle_unit_editor_panel_data.flag == 0
                        && g_battle_menu_active_turn_banner.team_kind != 1))) {
                battle_thread_start(g_battle_current_thread_id - 1, battle_menu_build_unit_status_list);
                battle_thread_set_parameters(
                    g_battle_current_thread_id - 1, (s32)&g_battle_menu_thread_menu_data[8], 0, 0);
            }
        }
        battle_gfx_draw_or_append_gpu_primitive(&frame_record->cursor);
        for (i = 0; i < 12; i++) {
            battle_gfx_draw_or_append_gpu_primitive(&frame_record->sprites[i]);
        }
        for (i = 14; i < 19; i++) {
            battle_gfx_draw_or_append_gpu_primitive(&frame_record->sprites[i]);
        }
        battle_gfx_draw_or_append_gpu_primitive(&frame_record->draw_modes[0]);
        battle_gfx_draw_or_append_gpu_primitive(&frame_record->sprites[12]);
        battle_gfx_draw_or_append_gpu_primitive(&frame_record->sprites[13]);
        battle_gfx_draw_or_append_gpu_primitive(&frame_record->draw_modes[1]);
        for (i = 0; i < 5; i++) {
            if ((u16)g_battle_unit_editor_panel_data.item_ids[i] != 0xFFFF) {
                battle_gfx_draw_or_append_gpu_primitive(&frame_record->sprites[19 + i]);
            }
        }
        battle_gfx_draw_or_append_gpu_primitive(&frame_record->draw_modes[2]);
        battle_gfx_draw_or_append_gpu_primitive(&frame_record->tiles[0]);
        battle_gfx_draw_or_append_gpu_primitive(&frame_record->tiles[1]);
        for (i = 0; i < 8; i++) {
            battle_gfx_draw_or_append_gpu_primitive(&frame_record->lines[i]);
        }
        frame++;
        battle_menu_submit_numeric_display_frame_primitives(frame_record->palette);
        battle_gfx_draw_or_append_gpu_primitive(&frame_record->cursor_mode);
    }
    g_battle_menu_hovered_unit_stats_display.y = 0xAA;
    g_battle_unit_summary_panel_rect.y = 0xAA;
    g_battle_menu_panel_fade_mode = 1;
    g_battle_unit_status_bounce_step = 6;
    battle_sound_set_effect_to_cancel();
    do {
        battle_thread_yield();
    } while (g_battle_unit_status_bounce_step != 0);
    battle_thread_start(0xA, battle_menu_selected_tile_info_display_thread);
    battle_thread_set_parameters(0xA, 0, 0, 0);
    g_battle_thread_contexts[10].task_id = 0x10;
    g_option_menu_submenu_state = 0;
    battle_menu_leave_status_screen_selection();
    battle_menu_build_idle_action_menu(g_battle_menu_current_id);
    battle_thread_exit_current();
}
