#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/event_equip.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* The packet, state and thread layouts are the shared fft/menu.h
 * records: the editor packets at g_equip_panel_selected_editor_packets / g_equip_panel_comparison_editor_packets, the
 * status-panel packets at g_equip_panel_selected_packets / g_equip_panel_comparison_packets, the editor states
 * g_equip_selected_unit_stat_summary / g_equip_panel_comparison_billboard, and the per-thread display record.
 * g_equip_panel_gauge_bar_colors holds the four Gouraud vertex colors of each bar. */

/*
 * EQUIP twin of debugchr_panel_run_unit_status_thread (DEBUGCHR
 * 0x801bf750): the same unit editor/status display thread, with thread slot
 * 0xD selecting the first packet set. When g_equip_item_numeric_thread_enabled is set, the first
 * numeric block is drawn as five separate entries (two through
 * menu_text_render_decimal_entry_list), as in world_menu_unit_status_banner_thread; the
 * zodiac/g_debugchr_editor_active branch of the DEBUGCHR version is absent.
 */
void equip_panel_run_unit_status_thread(void) {
    RECT rects[6];
    s32 anim_state;
    s32 cur_unit;
    s32 prev_unit;

    u8* number_image;                               /* sp60 */
    menu_status_panel_editor_packet_t* editor_base; /* sp68 */
    menu_number_entry_t* numeric_entries;           /* sp70 */
    menu_unit_status_record_t* state;               /* sp78 */
    s32 portrait_arg;                               /* sp80 */
    const RECT* portrait_rect;                      /* sp88 */
    palette_t* portrait_palettes;                   /* sp90 */
    u8* small_text_image;                           /* sp98 */
    u8* name_image;                                 /* spA0 */
    menu_status_panel_packet_t* panel_base;         /* spA8 */
    menu_unit_status_identity_t* unit_data;         /* spB0 */
    menu_status_panel_display_thread_t* thread;     /* spB8 */
    s32 shake;                                      /* spC0 */
    s32 dimmed;                                     /* spC8 */
    s32 suppress;                                   /* spD0 */

    s32 thread_id;
    s32 frame;
    s32 i;
    s32 panel_x;
    s32 panel_shake_y;
    s32 editor_x;
    s32 editor_shake_y;
    s32* window_width;
    s32 panel_offset_y;
    s32 panel_base_offset_y;
    s32 editor_offset_y;
    s32 editor_base_offset_y;
    s16 icon_index;
    glyph_blit_t* text_position;
    menu_status_panel_packet_t* panel;
    menu_status_panel_editor_packet_t* editor;
    SPRT* name_sprite;
    u8* number_image_lower;
    u8* small_text_image_2;
    u8* small_text_image_3;
    POLY_FT4* portrait_poly;

    suppress = 0;
    thread_set_current_task_id(THREAD_TASK_UNIT_STATUS_BANNER);
    g_menu_input_controller = script_get_controller_input_pointer(0) + 1;
    thread_id = g_thread_current_id;
    thread = (menu_status_panel_display_thread_t*)g_threads[thread_id].function_parameter_1;
    if (thread_id == EQUIP_THREAD_SLOT_UNIT_STATUS) {
        panel = g_equip_panel_selected_packets;
        editor = g_equip_panel_selected_editor_packets;
        number_image = g_equip_panel_selected_number_image;
        small_text_image = g_equip_panel_selected_large_number_image;
        name_image = g_equip_panel_selected_name_image;
        numeric_entries = g_equip_panel_selected_numeric_entries;
        state = &g_equip_selected_unit_stat_summary;
        unit_data = &g_equip_panel_selected_unit_data;
        portrait_rect = &g_equip_panel_selected_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        portrait_palettes = g_equip_panel_selected_portrait_palettes;
    } else {
        panel = g_equip_panel_comparison_packets;
        editor = g_equip_panel_comparison_editor_packets;
        number_image = g_equip_panel_comparison_number_image;
        small_text_image = g_equip_panel_comparison_large_number_image;
        name_image = g_equip_panel_comparison_name_image;
        numeric_entries = g_equip_panel_comparison_numeric_entries;
        state = &g_equip_panel_comparison_billboard;
        unit_data = &g_equip_panel_comparison_unit_data;
        portrait_rect = &g_equip_panel_comparison_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        portrait_palettes = g_equip_panel_comparison_portrait_palettes;
    }
    shake = 0;

    battle_menu_init_numeric_display_frame_primitives(&g_menu_portrait_origin, &editor->numeric_frame);
    battle_gfx_set_draw_mode_for_texture_page(&editor->draw_mode_a, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    battle_gfx_set_draw_mode_for_texture_page(&editor->draw_mode_b, MENU_TEXTURE_PAGE_FRAME);
    menu_init_sprite_array(&editor->label_sprites[0], 7, 0x7CBC);
    menu_init_sprite_array(&editor->value_sprites[0], 4, 0x7CBC);
    for (frame = 0; frame < 4; frame++) {
        menu_init_default_poly_ft4(&editor->portrait[frame]);
    }
    text_configure_sprite_vram(&rects[0], 0x38, 0x28, &editor->value_sprites[0], 2);
    text_configure_sprite_vram(&rects[1], 0x60, 0x10, &editor->value_sprites[1], 2);
    for (frame = 0; frame < 4; frame++) {
        gfx_init_image_loading(&editor->value_sprites[frame], g_menu_status_panel_numeric_geometry,
            &g_menu_portrait_origin, &g_equip_panel_editor_value_cells[frame]);
    }
    if (state->team_kind >= 4) {
        state->team_kind = 0;
    }
    if (g_battle_post_battle_unit_changes_active == 0) {
        mem_copy_bytes_nonempty(
            g_equip_panel_editor_mode_cell, &g_equip_panel_editor_mode_cells[state->team_kind], 0xC);
    } else {
        mem_copy_bytes_nonempty(g_equip_panel_editor_mode_cell, g_equip_panel_editor_mode_cells, 0xC);
    }
    for (frame = 0; frame < 7; frame++) {
        gfx_init_image_loading(&editor->label_sprites[frame], g_menu_status_panel_numeric_geometry,
            &g_menu_portrait_origin, &g_equip_panel_editor_label_cells[frame]);
    }
    mem_copy_bytes_nonempty(editor + 1, editor, sizeof(menu_status_panel_editor_packet_t));
    battle_gfx_set_draw_mode_for_texture_page(&panel->draw_mode_a, MENU_TEXTURE_PAGE_FRAME);
    battle_gfx_set_draw_mode_for_texture_page(&panel->draw_mode_b, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    battle_menu_init_numeric_display_frame_primitives(&g_equip_panel_frame_rect, &panel->numeric_frame);
    name_sprite = &panel->sprites[0];
    menu_init_sprite_array(name_sprite, 7, 0x7C3C);
    menu_init_default_poly_ft4(&panel->portrait);
    text_configure_sprite_vram(&rects[2], 0x58, 0x20, name_sprite, 0);
    text_configure_sprite_vram(&rects[3], 0x10, 0xA, &panel->sprites[1], 0);
    text_configure_sprite_vram(&rects[4], 0x10, 0xA, &panel->sprites[2], 0);
    text_configure_sprite_vram(&rects[5], 0x10, 0xA, &panel->sprites[3], 0);
    for (frame = 0; frame < 7; frame++) {
        gfx_init_image_loading(&panel->sprites[frame], g_menu_status_panel_numeric_geometry, &g_equip_panel_frame_rect,
            &g_equip_panel_sprite_cells[frame]);
    }
    gfx_init_image_loading(
        &panel->portrait, g_menu_status_panel_numeric_geometry, &g_equip_panel_frame_rect, g_equip_panel_portrait_cell);
    if (state->team_kind == 1) {
        panel->portrait.clut = 0x7FFD;
    } else {
        panel->portrait.clut = 0x7FBD;
    }
    panel->portrait.tpage = GetTPage(0, 1, 0x3C0, 0x100);
    mem_copy_bytes_nonempty(panel + 1, panel, sizeof(menu_status_panel_packet_t));
    frame = 0;
    anim_state = 0;
    prev_unit = state->unit_index;
    cur_unit = state->unit_index;
    for (;;) {
        s32 parity = frame & 1;
        panel = panel_base + parity;
        editor = editor_base + parity;
        if (state->gauges[MENU_UNIT_STATUS_GAUGE_CT].value > UNIT_TURN_CT_THRESHOLD) {
            state->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = UNIT_TURN_CT_THRESHOLD;
        }
        if (g_battle_post_battle_unit_changes_active == 0) {
            mem_copy_bytes_nonempty(
                g_equip_panel_editor_mode_cell, &g_equip_panel_editor_mode_cells[state->team_kind], 0xC);
        } else {
            mem_copy_bytes_nonempty(g_equip_panel_editor_mode_cell, g_equip_panel_editor_mode_cells, 0xC);
        }
        gfx_init_image_loading(&editor->label_sprites[6], g_menu_status_panel_numeric_geometry, &g_menu_portrait_origin,
            g_equip_panel_editor_mode_cell);
        if (state->team_kind == 1) {
            panel->portrait.clut = 0x7FFD;
        } else {
            panel->portrait.clut = 0x7FBD;
        }
        dimmed = 0;
        if (battle_thread_find_running_by_task(THREAD_TASK_STATUS_PANEL) != 0) {
            dimmed = g_menu_input_lock_mode != 0;
        }
        if (thread->dimmed != 0) {
            dimmed = 1;
        }
        for (i = 0; i < 4; i++) {
            editor->value_sprites[i].clut = g_equip_panel_editor_value_cluts[i][dimmed];
        }
        for (i = 0; i < 7; i++) {
            editor->label_sprites[i].clut = g_equip_panel_editor_label_cluts[i][dimmed];
        }
        i = 0;
        do {
            if (dimmed != 0) {
                panel->sprites[i].clut = 0x7D3C;
            } else {
                panel->sprites[i].clut = 0x7C3C;
            }
            i += 1;
        } while (i < 7);
        if (dimmed != 0) {
            menu_init_primitive_colors_dimmed(&editor->numeric_frame);
            menu_init_primitive_colors_dimmed(&panel->numeric_frame);
            if ((anim_state - 4) < 6U) {
                setRGB0(&editor->portrait[0], 0x20, 0x28, 0x38);
            } else {
                setRGB0(&editor->portrait[0], 0x40, 0x50, 0x70);
            }
            setRGB0(&editor->portrait[1], 0x20, 0x28, 0x38);
            setRGB0(&editor->portrait[2], 0x20, 0x28, 0x38);
            setRGB0(&editor->portrait[3], 0x20, 0x28, 0x38);
        } else {
            s32 shade;
            menu_init_primitive_colors_normal(&editor->numeric_frame);
            menu_init_primitive_colors_normal(&panel->numeric_frame);
            if ((anim_state - 4) >= 6U) {
                setRGB0(&editor->portrait[0], 0x80, 0x80, 0x80);
            } else {
                setRGB0(&editor->portrait[0], 0x40, 0x40, 0x40);
            }
            shade = 0x40;
            setRGB0(&editor->portrait[1], shade, shade, shade);
            setRGB0(&editor->portrait[2], shade, shade, shade);
            setRGB0(&editor->portrait[3], shade, shade, shade);
        }
        if (dimmed != 0) {
            setRGB0(&panel->portrait, 0x40, 0x40, 0x60);
        } else {
            setRGB0(&panel->portrait, 0x80, 0x80, 0x80);
        }
        g_menu_gfx_draw_offset_y = ((g_main_gfx_screen_polarity * 0xF0) != 0xF0) ? 0xF0 : 0;
        if ((thread->flags & MENU_STATUS_PANEL_SLIDE_DOWN) && (shake == 0)) {
            shake = 1;
        }
        if (thread->flags & MENU_STATUS_PANEL_SLIDE_UP) {
            if (shake == 0) {
                shake = 0xB;
            }
        }
        if (shake != 0) {
            if (shake < 0xA) {
                thread->shake_y = g_equip_panel_slide_down_y[shake];
                if (frame & 1) {
                    shake += 1;
                }
                if (shake >= 7) {
                    shake = 0;
                    thread->flags = thread->flags & MENU_STATUS_PANEL_HIDDEN;
                }
            } else if (shake >= 0xB) {
                thread->shake_y = g_equip_panel_slide_up_y[shake];
                if (frame & 1) {
                    shake += 1;
                }
                if (shake >= 0x11) {
                    shake = 0;
                    thread->flags = thread->flags & MENU_STATUS_PANEL_HIDDEN;
                }
            }
        }
        panel_x = thread->x;
        panel_offset_y = g_menu_gfx_draw_offset_y;
        panel->draw_offset_a.position.x = panel_x - 0x80;
        panel_shake_y = (u16)thread->shake_y;
        panel->draw_offset_a.position.y = panel_shake_y + panel_offset_y;
        SetDrawOffset(&panel->draw_offset_a.packet, (u16*)&panel->draw_offset_a.position);
        panel_base_offset_y = g_menu_gfx_draw_offset_y;
        panel->draw_offset_b.position.x = -0x80;
        panel->draw_offset_b.position.y = panel_base_offset_y;
        SetDrawOffset(&panel->draw_offset_b.packet, (u16*)&panel->draw_offset_b.position);
        editor_x = thread->x;
        editor_offset_y = g_menu_gfx_draw_offset_y;
        editor->draw_offset_a.position.x = editor_x - 0x80;
        editor_shake_y = (u16)thread->shake_y;
        editor->draw_offset_a.position.y = editor_shake_y + editor_offset_y;
        SetDrawOffset(&editor->draw_offset_a.packet, (u16*)&editor->draw_offset_a.position);
        editor_base_offset_y = g_menu_gfx_draw_offset_y;
        editor->draw_offset_b.position.x = -0x80;
        editor->draw_offset_b.position.y = editor_base_offset_y;
        SetDrawOffset(&editor->draw_offset_b.packet, (u16*)&editor->draw_offset_b.position);
        if (anim_state >= 0xA) {
            anim_state = 0;
            prev_unit = cur_unit;
            suppress = thread->flags & MENU_STATUS_PANEL_HIDDEN;
        }
        if (thread->work != 0) {
            if (anim_state == 0) {
                prev_unit = cur_unit;
                portrait_arg = thread->work;
                cur_unit = state->unit_index;
            } else {
                anim_state = 2;
                prev_unit = cur_unit;
                portrait_arg = thread->work;
                cur_unit = state->unit_index;
            }
        }
        if (cur_unit != prev_unit) {
            if (anim_state == 0) {
                anim_state = 2;
            }
        }
        menu_build_unit_portrait_transition(
            portrait_rect, &anim_state, &cur_unit, &prev_unit, portrait_palettes, &editor->portrait[0], portrait_arg);
        {
            CVECTOR* color;
            menu_unit_status_gauge_t* bar;

            color = g_equip_panel_gauge_bar_colors;
            bar = state->gauges;
            for (i = 0; i < 3; i++) {
                s32 x0;
                s32 w;
                s32 y;

                SetPolyG4(&editor->bars[i]);
                x0 = g_menu_portrait_origin.x + 0x2F;
                if (bar->max == 0) {
                    w = 4;
                } else {
                    w = (bar->value << 5) / bar->max;
                }
                if (w == 0) {
                    x0 = 0;
                }
                editor->bars[i].x0 = x0;
                y = i * 0xB + 0x18;
                editor->bars[i].y0 = g_menu_portrait_origin.y + y;
                editor->bars[i].x1 = x0 + w;
                editor->bars[i].y1 = g_menu_portrait_origin.y + y;
                editor->bars[i].x2 = x0;
                editor->bars[i].y2 = g_menu_portrait_origin.y + y + 3;
                editor->bars[i].x3 = x0 + w;
                editor->bars[i].y3 = g_menu_portrait_origin.y + y + 3;
                editor->bars[i].x2 -= 3;
                editor->bars[i].x3 -= 3;
                if (dimmed != 0) {
                    setRGB0(&editor->bars[i], color->r >> 1, color->g >> 1, color->b);
                    color++;
                    setRGB1(&editor->bars[i], color->r >> 1, color->g >> 1, color->b);
                    color++;
                    setRGB2(&editor->bars[i], color->r >> 1, color->g >> 1, color->b);
                    color++;
                    setRGB3(&editor->bars[i], color->r >> 1, color->g >> 1, color->b);
                    color++;
                } else {
                    setRGB0(&editor->bars[i], color->r, color->g, color->b);
                    color++;
                    setRGB1(&editor->bars[i], color->r, color->g, color->b);
                    color++;
                    setRGB2(&editor->bars[i], color->r, color->g, color->b);
                    color++;
                    setRGB3(&editor->bars[i], color->r, color->g, color->b);
                    color++;
                }
                bar++;
            }
        }
        panel->sprites[6].u0 = (unit_data->zodiac % 7) * 0x18;
        icon_index = unit_data->zodiac;
        panel->sprites[6].w = 0x18;
        panel->sprites[6].h = 0x14;
        panel->sprites[6].v0 = (icon_index / 7) * 0x14 + 0x2A;
        if ((frame == 0) || (thread->work != 0)) {
            if (state->list_index < 0) {
                numeric_entries[6].format = 0xC00;
            } else {
                numeric_entries[6].format = 2;
            }
            battle_clear_menu_render_buffer(number_image, 0x700);
            {
                s32* stride;
                glyph_blit_t* position;
                stride = &g_menu_text_state.pen.stride;
                *stride = 0x38;
                if (g_equip_item_numeric_thread_enabled == 0) {
                    battle_menu_draw_numeric_display_entries(
                        number_image, numeric_entries, (glyph_blit_t*)(stride - 2), 6);
                } else {
                    position = (glyph_blit_t*)(stride - 2);
                    menu_text_render_decimal_entry_list(number_image, &numeric_entries[10], position, 1);
                    menu_text_render_decimal_entry_list(number_image, &numeric_entries[12], position, 1);
                    battle_menu_draw_numeric_display_entries(number_image, &numeric_entries[11], position, 1);
                    battle_menu_draw_numeric_display_entries(number_image, &numeric_entries[13], position, 1);
                    battle_menu_draw_numeric_display_entries(number_image, &numeric_entries[4], position, 2);
                }
            }
            LoadImage(&rects[0], number_image);
            window_width = &g_menu_text_state.pen.stride;
            text_position = (glyph_blit_t*)(window_width - 2);
            number_image_lower = number_image + 0x400;
            *window_width = 0x60;
            battle_menu_draw_numeric_display_entries(number_image_lower, &numeric_entries[6], text_position, 4);
            LoadImage(&rects[1], number_image_lower);
            thread->work = 0;
            battle_clear_menu_render_buffer(name_image, 0x580);
            *window_width = 0x58;
            battle_menu_set_text_origin(0, 0);
            battle_menu_display_text_entry(unit_data->unit_index + TEXT_ID_UNIT_NAME_BASE, name_image, text_position);
            battle_menu_set_text_origin(0, 0x10);
            battle_menu_display_text_entry(unit_data->job_id + TEXT_ID_JOB_NAME_BASE, name_image, text_position);
            LoadImage(&rects[2], name_image);
            battle_clear_menu_render_buffer(small_text_image, 0xF0);
            *window_width = 0x10;
            battle_menu_set_text_origin(0, 0);
            battle_text_draw_large_number_glyphs(unit_data->brave, 0x202, small_text_image, text_position);
            LoadImage(&rects[3], small_text_image);
            battle_menu_set_text_origin(0, 0);
            small_text_image_2 = small_text_image + 0x50;
            battle_text_draw_large_number_glyphs(unit_data->faith, 0x202, small_text_image_2, text_position);
            LoadImage(&rects[4], small_text_image_2);
            if (g_battle_post_battle_unit_changes_active == 0) {
                battle_menu_set_text_origin(0, 0);
                small_text_image_3 = small_text_image + 0xA0;
                battle_text_draw_large_number_glyphs(
                    unit_data->roster_slot + 1, 0x4002, small_text_image_3, text_position);
                LoadImage(&rects[5], small_text_image_3);
            } else {
                LoadImage(&rects[5], small_text_image + 0xA0);
            }
        }
        if (!(thread->flags & MENU_STATUS_PANEL_HIDDEN)) {
            gfx_draw_or_append_gpu_primitive(&editor->draw_offset_b);
            gfx_draw_or_append_gpu_primitive(&editor->value_sprites[0]);
            gfx_draw_or_append_gpu_primitive(&editor->value_sprites[1]);
            gfx_draw_or_append_gpu_primitive(&editor->draw_mode_a);
            for (i = 0; i < 3; i++) {
                gfx_draw_or_append_gpu_primitive(&editor->bars[i]);
            }
            gfx_draw_or_append_gpu_primitive(&editor->value_sprites[2]);
            gfx_draw_or_append_gpu_primitive(&editor->value_sprites[3]);
            for (i = 0; i < 7; i++) {
                gfx_draw_or_append_gpu_primitive(&editor->label_sprites[i]);
            }
            gfx_draw_or_append_gpu_primitive(&editor->draw_mode_b);
            if (suppress == 0) {
                if (anim_state >= 5) {
                    gfx_draw_or_append_gpu_primitive(&editor->portrait[2]);
                    gfx_draw_or_append_gpu_primitive(&editor->portrait[3]);
                    gfx_draw_or_append_gpu_primitive(&editor->portrait[1]);
                }
                gfx_draw_or_append_gpu_primitive(&editor->portrait[0]);
            }
            menu_submit_numeric_display_frame_primitives(&editor->numeric_frame);
            gfx_draw_or_append_gpu_primitive(&editor->draw_offset_a);
            gfx_draw_or_append_gpu_primitive(&panel->draw_offset_b);
            portrait_poly = &panel->portrait;
            SetSemiTrans(portrait_poly, 1);
            gfx_draw_or_append_gpu_primitive(portrait_poly);
            for (i = 3; i >= 0; i--) {
                gfx_draw_or_append_gpu_primitive(&panel->sprites[i]);
            }
            gfx_draw_or_append_gpu_primitive(&panel->draw_mode_b);
            for (i = 4; i < 7; i++) {
                gfx_draw_or_append_gpu_primitive(&panel->sprites[i]);
            }
            gfx_draw_or_append_gpu_primitive(&panel->draw_mode_a);
            menu_submit_numeric_display_frame_primitives(&panel->numeric_frame);
            gfx_draw_or_append_gpu_primitive(&panel->draw_offset_a);
        }
        thread_yield();
        if (battle_thread_get_current_parameter_3() != 0) {
            break;
        }
        frame++;
    }
    thread_yield();
    battle_gfx_free_tpage7_vram(&rects[0]);
    battle_gfx_free_tpage7_vram(&rects[1]);
    battle_gfx_free_tpage7_vram(&rects[2]);
    battle_gfx_free_tpage7_vram(&rects[3]);
    battle_gfx_free_tpage7_vram(&rects[4]);
    battle_gfx_free_tpage7_vram(&rects[5]);
    thread_exit_current();
}
