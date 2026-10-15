#include "fft/battle_menu.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/script.h"
#include "fft/thread.h"
/* REQUIRE twin of debugchr_panel_run_unit_status_thread: the same doubled
 * 0x30C editor / 0x1D8 panel packets (fft/menu.h), without the shake
 * offsets, the bar division and the zodiac branch.
 */
#include "fft/battle_script.h"
#include "fft/event_require.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

void require_panel_run_unit_status_thread(void) {
    RECT rects[6];
    s32 anim_state;
    s32 cur_unit;
    s32 prev_unit;

    u8* number_image;
    menu_status_panel_editor_packet_t* editor_base;
    menu_number_entry_t* numeric_entries;
    menu_unit_status_record_t* state;
    s32 portrait_arg;
    const RECT* portrait_rect;
    palette_t* portrait_palettes;
    u8* small_text_image;
    u8* name_image;
    menu_status_panel_packet_t* panel_base;
    menu_active_unit_t* unit_data;
    menu_status_panel_display_thread_t* thread;
    s32 frame;
    s32 dimmed;
    s32 suppress;

    s32 thread_id;
    s32 i;
    s32* window_width;
    s32 panel_x;
    s32 editor_x;
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
    if (thread_id == THREAD_SCREEN_SLOT_UNIT_STATUS) {
        panel = g_require_panel_selected_packets;
        editor = g_require_panel_selected_editor_packets;
        number_image = g_require_panel_selected_number_image;
        small_text_image = g_require_panel_selected_large_number_image;
        name_image = g_require_panel_selected_name_image;
        numeric_entries = g_require_panel_selected_numeric_entries;
        state = &g_require_panel_selected_billboard;
        unit_data = &g_require_active_unit_data;
        portrait_rect = &g_require_panel_selected_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        portrait_palettes = g_require_panel_selected_portrait_palettes;
    } else {
        panel = g_require_panel_comparison_packets;
        editor = g_require_panel_comparison_editor_packets;
        number_image = g_require_panel_comparison_number_image;
        small_text_image = g_require_panel_comparison_large_number_image;
        name_image = g_require_panel_comparison_name_image;
        numeric_entries = g_require_panel_comparison_numeric_entries;
        state = &g_require_panel_comparison_billboard;
        unit_data = &g_require_panel_comparison_unit_data;
        portrait_rect = &g_require_panel_comparison_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        portrait_palettes = g_require_panel_comparison_portrait_palettes;
    }

    menu_init_numeric_display_frame_primitives(&g_gfx_portrait_origin, &editor->numeric_frame);
    gfx_set_draw_mode_for_texture_page(&editor->draw_mode_a, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    gfx_set_draw_mode_for_texture_page(&editor->draw_mode_b, MENU_TEXTURE_PAGE_FRAME);
    menu_init_sprite_array(&editor->label_sprites[0], 7, 0x7CBC);
    menu_init_sprite_array(&editor->value_sprites[0], 4, 0x7CBC);
    for (frame = 0; frame < 4; frame++) {
        menu_init_default_poly_ft4((&editor->portrait[frame]));
    }
    text_configure_sprite_vram(&rects[0], 0x38, 0x28, &editor->value_sprites[0], 2);
    text_configure_sprite_vram(&rects[1], 0x60, 0x10, &editor->value_sprites[1], 2);
    for (frame = 0; frame < 4; frame++) {
        gfx_init_image_loading(&editor->value_sprites[frame], g_require_editor_numeric_geometry, &g_gfx_portrait_origin,
            &g_require_panel_editor_value_cells[frame]);
    }
    if (state->team_kind >= 4) {
        state->team_kind = 0;
    }
    if (g_battle_post_battle_unit_changes_active == 0) {
        mem_copy_bytes_nonempty(
            g_require_panel_editor_mode_cell, &g_require_panel_editor_mode_cells[state->team_kind], 0xC);
    } else {
        mem_copy_bytes_nonempty(g_require_panel_editor_mode_cell, g_require_panel_editor_mode_cells, 0xC);
    }
    for (frame = 0; frame < 7; frame++) {
        gfx_init_image_loading(&editor->label_sprites[frame], g_require_editor_numeric_geometry, &g_gfx_portrait_origin,
            &g_require_panel_editor_label_cells[frame]);
    }
    mem_copy_bytes_nonempty(editor + 1, editor, sizeof(menu_status_panel_editor_packet_t));
    gfx_set_draw_mode_for_texture_page(&panel->draw_mode_a, MENU_TEXTURE_PAGE_FRAME);
    gfx_set_draw_mode_for_texture_page(&panel->draw_mode_b, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    menu_init_numeric_display_frame_primitives(&g_require_panel_frame_rect, &panel->numeric_frame);
    name_sprite = &panel->sprites[0];
    menu_init_sprite_array(name_sprite, 7, 0x7C3C);
    menu_init_default_poly_ft4(&panel->portrait);
    text_configure_sprite_vram(&rects[2], 0x58, 0x20, name_sprite, 0);
    text_configure_sprite_vram(&rects[3], 0x10, 0xA, &panel->sprites[1], 0);
    text_configure_sprite_vram(&rects[4], 0x10, 0xA, &panel->sprites[2], 0);
    text_configure_sprite_vram(&rects[5], 0x10, 0xA, &panel->sprites[3], 0);
    for (frame = 0; frame < 7; frame++) {
        gfx_init_image_loading(&panel->sprites[frame], g_require_editor_numeric_geometry, &g_require_panel_frame_rect,
            &g_require_panel_sprite_cells[frame]);
    }
    gfx_init_image_loading(&panel->portrait, g_require_editor_numeric_geometry, &g_require_panel_frame_rect,
        g_require_panel_portrait_cell);
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
                g_require_panel_editor_mode_cell, &g_require_panel_editor_mode_cells[state->team_kind], 0xC);
        } else {
            mem_copy_bytes_nonempty(g_require_panel_editor_mode_cell, g_require_panel_editor_mode_cells, 0xC);
        }
        gfx_init_image_loading(&editor->label_sprites[6], g_require_editor_numeric_geometry, &g_gfx_portrait_origin,
            g_require_panel_editor_mode_cell);
        if (state->team_kind == 1) {
            panel->portrait.clut = 0x7FFD;
        } else {
            panel->portrait.clut = 0x7FBD;
        }
        dimmed = 0;
        if (thread_find_running_by_task(THREAD_TASK_STATUS_PANEL) != 0) {
            dimmed = g_menu_input_lock_mode != 0;
        }
        if (thread->dimmed != 0) {
            dimmed = 1;
        }
        for (i = 0; i < 4; i++) {
            editor->value_sprites[i].clut = g_require_panel_editor_value_cluts[i][dimmed];
        }
        for (i = 0; i < 7; i++) {
            editor->label_sprites[i].clut = g_require_panel_editor_label_cluts[i][dimmed];
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
        panel_x = thread->x;
        panel->draw_offset_a.position.x = panel_x - 0x80;
        g_require_gfx_draw_offset_y = ((g_main_gfx_screen_polarity * 0xF0) != 0xF0) ? 0xF0 : 0;
        panel->draw_offset_a.position.y = (u16)thread->shake_y + g_require_gfx_draw_offset_y;
        SetDrawOffset(&panel->draw_offset_a.packet, (u16*)&panel->draw_offset_a.position);
        panel->draw_offset_b.position.x = -0x80;
        panel->draw_offset_b.position.y = g_require_gfx_draw_offset_y;
        SetDrawOffset(&panel->draw_offset_b.packet, (u16*)&panel->draw_offset_b.position);
        editor_x = thread->x;
        editor->draw_offset_a.position.x = editor_x - 0x80;
        editor->draw_offset_a.position.y = (u16)thread->shake_y + g_require_gfx_draw_offset_y;
        SetDrawOffset(&editor->draw_offset_a.packet, (u16*)&editor->draw_offset_a.position);
        editor->draw_offset_b.position.x = -0x80;
        editor->draw_offset_b.position.y = g_require_gfx_draw_offset_y;
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
        gfx_build_portrait_transition_primitives(
            portrait_rect, &anim_state, &cur_unit, &prev_unit, portrait_palettes, &editor->portrait[0], portrait_arg);
        {
            CVECTOR* color;

            color = g_require_panel_gauge_bar_colors;
            for (i = 0; i < 3; i++) {
                s32 x0;
                s32 x1;
                s32 y;

                SetPolyG4(&editor->bars[i]);
                x1 = g_gfx_portrait_origin.x;
                x0 = x1 + 0x2F;
                x1 += 0x4F;
                editor->bars[i].x0 = x0;
                y = i * 0xB + 0x18;
                editor->bars[i].y0 = g_gfx_portrait_origin.y + y;
                editor->bars[i].x1 = x1;
                editor->bars[i].y1 = g_gfx_portrait_origin.y + y;
                editor->bars[i].x2 = x0;
                editor->bars[i].y2 = g_gfx_portrait_origin.y + y + 3;
                editor->bars[i].x3 = x1;
                editor->bars[i].y3 = g_gfx_portrait_origin.y + y + 3;
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
            menu_clear_render_buffer(number_image, 0x700);
            window_width = &g_menu_text_state.pen.stride;
            text_position = (glyph_blit_t*)(window_width - 2);
            *window_width = 0x38;
            menu_draw_numeric_display_entries(number_image, numeric_entries, text_position, 6);
            LoadImage(&rects[0], (u32*)number_image);
            number_image_lower = number_image + 0x400;
            *window_width = 0x60;
            menu_draw_numeric_display_entries(number_image_lower, &numeric_entries[8], text_position, 2);
            LoadImage(&rects[1], (u32*)number_image_lower);
            thread->work = 0;
            menu_clear_render_buffer(name_image, 0x580);
            *window_width = 0x58;
            menu_set_text_origin(0, 0);
            menu_display_text_entry(unit_data->name_id + TEXT_ID_UNIT_NAME_BASE, name_image, text_position);
            menu_set_text_origin(0, 0x10);
            menu_display_text_entry(unit_data->job_id + TEXT_ID_JOB_NAME_BASE, name_image, text_position);
            LoadImage(&rects[2], (u32*)name_image);
            menu_clear_render_buffer(small_text_image, 0xF0);
            *window_width = 0x10;
            menu_set_text_origin(0, 0);
            text_draw_large_number_glyphs_variant(unit_data->brave, 0x202, small_text_image, text_position);
            LoadImage(&rects[3], (u32*)small_text_image);
            menu_set_text_origin(0, 0);
            small_text_image_2 = small_text_image + 0x50;
            text_draw_large_number_glyphs_variant(unit_data->faith, 0x202, small_text_image_2, text_position);
            LoadImage(&rects[4], (u32*)small_text_image_2);
            if (g_battle_post_battle_unit_changes_active == 0) {
                menu_set_text_origin(0, 0);
                small_text_image_3 = small_text_image + 0xA0;
                text_draw_large_number_glyphs_variant(
                    unit_data->battle_id + 1, 0x4002, small_text_image_3, text_position);
                LoadImage(&rects[5], (u32*)small_text_image_3);
            } else {
                LoadImage(&rects[5], (u32*)(small_text_image + 0xA0));
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
        if (thread_get_current_parameter_3_variant() != 0) {
            break;
        }
        frame += 1;
    }
    thread_yield();
    gfx_free_texture_rect(&rects[0]);
    gfx_free_texture_rect(&rects[1]);
    gfx_free_texture_rect(&rects[2]);
    gfx_free_texture_rect(&rects[3]);
    gfx_free_texture_rect(&rects[4]);
    gfx_free_texture_rect(&rects[5]);
    thread_exit_current();
}
