#include "fft/battle_action.h"
#include "fft/battle_event.h"
#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

void menu_run_numeric_display_panel_thread(void) {
    menu_panel_record_t* records;
    menu_panel_record_t* record;
    menu_unit_stat_number_images_t* number_images;
    s32 i;
    s32 frame;

    record = g_menu_numeric_display_panel_records;
    records = record;
    thread_set_current_task_id(THREAD_TASK_NUMERIC_DISPLAY_PANEL);
    gfx_set_draw_mode_for_texture_page(&record->mode0, MENU_TEXTURE_PAGE_FRAME);
    gfx_set_draw_mode_for_texture_page(&record->mode1, MENU_TEXTURE_PAGE_TEXT_IMAGES);
    menu_init_scaled_draw_area_packets(&record->draw_areas);
    number_images = menu_alloc_memory(sizeof(*number_images));
    menu_clear_render_buffer(number_images, sizeof(*number_images));
    g_menu_text_state.pen.stride = 0x14;
    menu_set_text_origin(0, 0);
    menu_draw_numeric_display_entries(
        number_images->mobility, g_menu_numeric_display_entries, &g_menu_text_state.pen, 3);
    g_menu_text_state.pen.stride = 0x40;
    menu_set_text_origin(0, 0);
    menu_draw_numeric_display_entries(
        number_images->weapons, &g_menu_numeric_display_entries[3], &g_menu_text_state.pen, 4);
    g_menu_text_state.pen.stride = 0x64;
    menu_set_text_origin(0, 0);
    menu_draw_numeric_display_entries(
        number_images->combat, &g_menu_numeric_display_entries[7], &g_menu_text_state.pen, 8);
    LoadImage(&g_menu_numeric_display_text_rect_a, (u32*)number_images->mobility);
    LoadImage(&g_menu_numeric_display_text_rect_b, (u32*)number_images->weapons);
    LoadImage(&g_menu_numeric_display_text_rect_c, (u32*)number_images->combat);
    thread_wait_frames(1);
    menu_free_memory(number_images);
    for (i = 0; i < 18; i++) {
        menu_init_sprt_variant(&record->sprites[i]);
        gfx_init_image_loading(&record->sprites[i], &g_menu_texture_origin, &g_menu_numeric_display_frame_rect,
            &g_menu_numeric_display_image_params[i]);
    }
    mem_copy_bytes_nonempty(&record[1], record, sizeof(*record));
    record[0].palette = &g_menu_numeric_display_palettes[0].primitives;
    record[1].palette = &g_menu_numeric_display_palettes[1].primitives;
    menu_init_numeric_display_frame_primitives(&g_menu_numeric_display_frame_rect, record[0].palette);
    menu_init_numeric_display_frame_primitives(&g_menu_numeric_display_frame_rect, record[1].palette);
    for (frame = 0;; frame++) {
        thread_yield();
        record = &records[frame & 1];
        gfx_build_scaled_draw_area_pair_swapped(
            &record->draw_areas, &g_menu_numeric_display_panel_texture_window, frame, g_main_gfx_screen_polarity);
        if (thread_get_current_parameter_3_variant() != 0) {
            break;
        }
        if (thread_is_running_asm_variant(THREAD_SLOT_MENU_GRANDCHILD) != 0) {
            for (i = 17; i >= 0; i--) {
                record->sprites[i].clut = 0x7D3C;
            }
            record->sprites[10].clut = 0x7EBC;
            menu_init_primitive_colors_dimmed(record->palette);
        } else {
            for (i = 17; i >= 0; i--) {
                record->sprites[i].clut = 0x7C3C;
            }
            record->sprites[10].clut = 0x7D7C;
            record->sprites[14].clut = 0x7FFC;
            record->sprites[15].clut = 0x7FFC;
            record->sprites[16].clut = 0x7FFC;
            menu_init_primitive_colors_normal(record->palette);
        }
        gfx_draw_or_append_gpu_primitive(&record->draw_areas.areas[1]);
        for (i = 0; i < 14; i++) {
            gfx_draw_or_append_gpu_primitive(&record->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&record->sprites[17]);
        gfx_draw_or_append_gpu_primitive(&record->mode0);
        for (; i < 17; i++) {
            gfx_draw_or_append_gpu_primitive(&record->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&record->mode1);
        menu_submit_numeric_display_frame_primitives(record->palette);
        gfx_draw_or_append_gpu_primitive(&record->draw_areas.areas[0]);
    }
    thread_yield();
    thread_exit_current();
}

/* Build and redraw the unit editor panel while its menu thread is active. */
void menu_run_unit_editor_panel_thread(void) {
    /* The frame reserves 0x100 bytes for the first copy's line-box
       primitives, the stride of the palette records (WORLD 0x80195ed0). */
    menu_palette_slot_t palette;
    menu_unit_editor_frame_t* records;
    menu_unit_editor_frame_t* frame_record;
    LINE_F2* line;
    u8* buffer;
    /* One variable for the label text id and, in the frame loop, whether the
       status thread runs: the target keeps both in one callee-saved register. */
    s32 value;
    s32 frame;
    s32 i;

    menu_open_debugchr_unit_editor();
    thread_set_current_task_id(THREAD_TASK_UNIT_EDITOR_PANEL);
    battle_menu_enter_status_screen_selection();
    g_menu_overlay_state = 1;
    g_menu_panel_fade_mode = MENU_PANEL_FADE_OUT;
    thread_set_parameters(THREAD_SLOT_TILE_INFO_PANEL, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
    menu_record_selection_for_selected_unit();
    menu_set_modal_open();
    do {
        thread_yield();
    } while (g_menu_hovered_stats_frame_count < 2);
    menu_clear_modal_open();

    frame_record = g_menu_unit_editor_frames;
    records = frame_record;
    menu_init_scaled_draw_area_packets(&frame_record->draw_areas);
    g_menu_hovered_unit_stats_rect.y = 0x1A;
    g_menu_unit_summary_panel_rect.y = 0x1A;
    gfx_set_draw_mode_for_texture_page(&frame_record->draw_modes[0], MENU_TEXTURE_PAGE_FRAME);
    gfx_set_draw_mode_for_texture_page(&frame_record->draw_modes[1], MENU_TEXTURE_PAGE_TEXT_IMAGES);
    gfx_set_draw_mode_for_texture_page(&frame_record->draw_modes[2], MENU_TEXTURE_PAGE_ITEM_ICONS);
    SetTile(&frame_record->tiles[0]);
    SetSemiTrans(&frame_record->tiles[0], 1);
    setRGB0(&frame_record->tiles[0], 0x30, 0x30, 0x30);
    frame_record->tiles[0].w = 0x10;
    frame_record->tiles[0].h = 0x5A;
    frame_record->tiles[0].x0 = g_menu_unit_editor_panel_rect.x;
    frame_record->tiles[0].y0 = g_menu_unit_editor_panel_rect.y + 1;
    mem_copy_bytes_nonempty(&frame_record->tiles[1], &frame_record->tiles[0], sizeof(TILE));
    frame_record->tiles[0].x0 += 0x12;
    frame_record->tiles[1].x0 += 0x7C;
    line = frame_record->lines;
    for (i = 0; i < 8 * MENU_UNIT_EDITOR_LINE_SIZE; i += MENU_UNIT_EDITOR_LINE_SIZE) {
        SetLineF2(line);
        SetSemiTrans(line, 1);
        setRGB0(line, g_menu_unit_editor_line_colors[i + MENU_UNIT_EDITOR_LINE_R],
            g_menu_unit_editor_line_colors[i + MENU_UNIT_EDITOR_LINE_G],
            g_menu_unit_editor_line_colors[i + MENU_UNIT_EDITOR_LINE_B]);
        setXY2(line, g_menu_unit_editor_line_colors[i + MENU_UNIT_EDITOR_LINE_X0] + g_menu_unit_editor_panel_rect.x,
            g_menu_unit_editor_line_colors[i + MENU_UNIT_EDITOR_LINE_Y0] + g_menu_unit_editor_panel_rect.y,
            g_menu_unit_editor_line_colors[i + MENU_UNIT_EDITOR_LINE_X1] + g_menu_unit_editor_panel_rect.x,
            g_menu_unit_editor_line_colors[i + MENU_UNIT_EDITOR_LINE_Y1] + g_menu_unit_editor_panel_rect.y);
        line++;
    }
    menu_init_sprite_array(&frame_record->sprites[0], 0xC, 0x7D7C);
    menu_init_sprite_array(&frame_record->sprites[12], 7, 0x7C3C);

    /* Each arm copies all four rows itself; the compiler cross-jumps the
       common tails, leaving only the first call's table loads in the arms. */
    if (g_menu_selected_unit_fields.state.two_hands != 0) {
        mem_copy_bytes_nonempty(&g_menu_unit_editor_sprite_image_params_a[2], g_menu_unit_editor_row_image_params_b,
            2 * sizeof(image_load_parameters_t));
        mem_copy_bytes_nonempty(&g_menu_unit_editor_sprite_image_params_a[14],
            &g_menu_unit_editor_row_image_params_b[2], 2 * sizeof(image_load_parameters_t));
        mem_copy_bytes_nonempty(&g_menu_unit_editor_sprite_image_params_b[2], g_menu_unit_editor_row_image_params_b,
            2 * sizeof(image_load_parameters_t));
        mem_copy_bytes_nonempty(&g_menu_unit_editor_sprite_image_params_b[14],
            &g_menu_unit_editor_row_image_params_b[2], 2 * sizeof(image_load_parameters_t));
    } else {
        mem_copy_bytes_nonempty(&g_menu_unit_editor_sprite_image_params_a[2], g_menu_unit_editor_row_image_params_a,
            2 * sizeof(image_load_parameters_t));
        mem_copy_bytes_nonempty(&g_menu_unit_editor_sprite_image_params_a[14],
            &g_menu_unit_editor_row_image_params_a[2], 2 * sizeof(image_load_parameters_t));
        mem_copy_bytes_nonempty(&g_menu_unit_editor_sprite_image_params_b[2], g_menu_unit_editor_row_image_params_a,
            2 * sizeof(image_load_parameters_t));
        mem_copy_bytes_nonempty(&g_menu_unit_editor_sprite_image_params_b[14],
            &g_menu_unit_editor_row_image_params_a[2], 2 * sizeof(image_load_parameters_t));
    }
    /* These sprite and rectangle records share the fields read by the image loader. */
    if (g_menu_selected_unit_fields.state.generic_monster == 0) {
        for (i = 0; i < 19; i++) {
            gfx_init_image_loading(&frame_record->sprites[i], &g_menu_texture_origin, &g_menu_unit_editor_panel_rect,
                &g_menu_unit_editor_sprite_image_params_a[i]);
        }
    } else {
        for (i = 0; i < 19; i++) {
            gfx_init_image_loading(&frame_record->sprites[i], &g_menu_texture_origin, &g_menu_unit_editor_panel_rect,
                &g_menu_unit_editor_sprite_image_params_b[i]);
        }
    }
    menu_init_sprite_array(&frame_record->sprites[19], 5, 0x7CFC);
    for (i = 0; i < 5; i++) {
        gfx_init_image_loading(&frame_record->sprites[19 + i], &g_gfx_item_icon_image_location,
            &g_menu_unit_editor_panel_rect, &g_menu_unit_editor_item_icon_image_params[i]);
        gfx_set_item_icon_sprite(&frame_record->sprites[19 + i], g_menu_selected_unit_fields.state.equipment[i]);
    }

    buffer = menu_alloc_buffer(0xC80);
    menu_clear_render_buffer(buffer, 0xC80);
    g_menu_text_state.pen.stride = 0x50;
    menu_set_text_origin(0, 0);
    for (i = 0; i < 5; i++) {
        value = g_menu_selected_unit_fields.state.equipment[i] + TEXT_ID_ITEM_NAME_BASE;
        if ((u16)g_menu_selected_unit_fields.state.equipment[i] == 0xFFFF) {
            g_menu_text_state.pen.rect.y += 0x10;
        } else {
            menu_display_text_entry(value, buffer, &g_menu_text_state.pen);
            g_menu_text_state.pen.rect.y += 0x10;
        }
    }
    LoadImage(&g_menu_unit_editor_text_rect_a, (u32*)buffer);
    thread_yield();
    menu_clear_render_buffer(buffer, 0xC80);
    g_menu_text_state.pen.stride = 0x50;
    menu_set_text_origin(0, 0);
    for (i = 0; i < 2; i++) {
        value = g_menu_selected_unit_fields.state.ability_ids[i] + TEXT_ID_SKILLSET_NAME_SOURCE_BASE;
        if (g_menu_selected_unit_fields.state.generic_monster != 0) {
            value = g_menu_selected_unit_fields.state.ability_ids[i] + TEXT_ID_ABILITY_NAME_BASE;
        }
        if ((u16)g_menu_selected_unit_fields.state.ability_ids[i] == MENU_STATUS_PANEL_LABEL_NONE) {
            g_menu_text_state.pen.rect.y += 0x10;
        } else {
            menu_display_text_entry(value, buffer, &g_menu_text_state.pen);
            g_menu_text_state.pen.rect.y += 0x10;
        }
    }
    for (i = 2; i < 5; i++) {
        value = g_menu_selected_unit_fields.state.ability_ids[i] + TEXT_ID_ABILITY_NAME_BASE;
        if ((u16)g_menu_selected_unit_fields.state.ability_ids[i] == MENU_STATUS_PANEL_LABEL_NONE) {
            g_menu_text_state.pen.rect.y += 0x10;
        } else {
            menu_display_text_entry(value, buffer, &g_menu_text_state.pen);
            g_menu_text_state.pen.rect.y += 0x10;
        }
    }
    if (g_menu_selected_unit_fields.state.generic_monster != 0) {
        for (i = 0; i < 5; i++) {
            if ((u16)g_menu_selected_unit_fields.state.ability_ids[i] == MENU_STATUS_PANEL_LABEL_NONE) {
                frame_record->sprites[7 + i].x0 -= 0x200;
            }
        }
    }
    LoadImage(&g_menu_unit_editor_text_rect_b, (u32*)buffer);
    thread_yield();
    menu_free_memory(buffer);

    mem_copy_bytes_nonempty(&records[1], frame_record, sizeof(menu_unit_editor_frame_t));
    records[0].palette = &palette.primitives;
    records[1].palette = &g_menu_unit_editor_palette;
    menu_init_numeric_display_frame_primitives(&g_menu_unit_editor_panel_rect, records[0].palette);
    menu_init_numeric_display_frame_primitives(&g_menu_unit_editor_panel_rect, records[1].palette);
    thread_start(THREAD_SLOT_TILE_INFO_PANEL, menu_run_numeric_display_panel_thread);
    thread_set_parameters(THREAD_SLOT_TILE_INFO_PANEL, 0, 0, 0);
    thread_yield();

    frame = 0;
    for (;;) {
        thread_yield();
        frame_record = &records[frame & 1];
        gfx_build_scaled_draw_area_pair_swapped(
            &frame_record->draw_areas, &g_menu_unit_editor_cursor_rect, frame, g_main_gfx_screen_polarity);
        if (g_menu_input_lock_mode == 0 && g_menu_help_open == 0 && thread_get_current_parameter_3_variant() != 0) {
            break;
        }
        value = thread_is_running_asm_variant(g_thread_current_id - 1);
        for (i = 0; i < 19; i++) {
            frame_record->sprites[i].clut = g_menu_unit_editor_sprite_clut_pairs[i][value];
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
            menu_init_primitive_colors_normal(frame_record->palette);
        } else {
            menu_init_primitive_colors_dimmed(frame_record->palette);
        }
        if (menu_can_accept_input_variant() != 0) {
            if (g_script_event_input & PSX_PAD_CROSS) {
                thread_set_parameters(THREAD_SLOT_TILE_INFO_PANEL, 0, 0, 1);
                break;
            }
            if ((g_script_event_input & PSX_PAD_CIRCLE) && thread_is_running_asm_variant(THREAD_SLOT_MENU_BUILDING) == 0
                && (unit_has_any_current_status(g_menu_selected_unit.battle_id) != 0
                    || (g_menu_selected_unit_fields.state.generic_monster == 0
                        && g_menu_selected_unit_status.team_kind != 1))) {
                thread_start(g_thread_current_id - 1, menu_build_unit_status_list);
                thread_set_parameters(
                    g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_STATUS_LIST], 0, 0);
            }
        }
        gfx_draw_or_append_gpu_primitive(&frame_record->draw_areas.areas[1]);
        for (i = 0; i < 12; i++) {
            gfx_draw_or_append_gpu_primitive(&frame_record->sprites[i]);
        }
        for (i = 14; i < 19; i++) {
            gfx_draw_or_append_gpu_primitive(&frame_record->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&frame_record->draw_modes[0]);
        gfx_draw_or_append_gpu_primitive(&frame_record->sprites[12]);
        gfx_draw_or_append_gpu_primitive(&frame_record->sprites[13]);
        gfx_draw_or_append_gpu_primitive(&frame_record->draw_modes[1]);
        for (i = 0; i < 5; i++) {
            if ((u16)g_menu_selected_unit_fields.state.equipment[i] != 0xFFFF) {
                gfx_draw_or_append_gpu_primitive(&frame_record->sprites[19 + i]);
            }
        }
        gfx_draw_or_append_gpu_primitive(&frame_record->draw_modes[2]);
        gfx_draw_or_append_gpu_primitive(&frame_record->tiles[0]);
        gfx_draw_or_append_gpu_primitive(&frame_record->tiles[1]);
        for (i = 0; i < 8; i++) {
            gfx_draw_or_append_gpu_primitive(&frame_record->lines[i]);
        }
        frame++;
        menu_submit_numeric_display_frame_primitives(frame_record->palette);
        gfx_draw_or_append_gpu_primitive(&frame_record->draw_areas.areas[0]);
    }
    g_menu_hovered_unit_stats_rect.y = 0xAA;
    g_menu_unit_summary_panel_rect.y = 0xAA;
    g_menu_panel_fade_mode = MENU_PANEL_FADE_IN;
    g_menu_unit_status_bounce_step = 6;
    sound_queue_sfx_cancel();
    do {
        thread_yield();
    } while (g_menu_unit_status_bounce_step != 0);
    thread_start(THREAD_SLOT_TILE_INFO_PANEL, menu_run_selected_tile_info_display_thread);
    thread_set_parameters(THREAD_SLOT_TILE_INFO_PANEL, 0, 0, 0);
    g_thread_contexts[THREAD_SLOT_TILE_INFO_PANEL].task_id = THREAD_TASK_TILE_INFO_PANEL;
    g_menu_overlay_state = 0;
    battle_menu_leave_status_screen_selection();
    menu_build_idle_action_menu_variant(g_menu_current_id);
    thread_exit_current();
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * menu_run_unit_editor_panel_thread and menu_can_accept_input_variant. Nothing on the
 * disc calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void menu_noop_3(void) {
}
