#include "fft/event_bunit.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* The packet, state and thread layouts are the shared fft/menu.h
 * records of the DEBUGCHR twin menu_run_unit_status_thread_variant. */

/* Thread task 0x39: the character editor and status panels. BUNIT's body (DEBUGCHR has its own). */
void menu_run_unit_status_thread_variant(void) {
    RECT rects[6];
    s32 anim_state;
    s32 cur_unit;
    s32 prev_unit;

    u8* number_pixels;
    menu_status_panel_editor_packet_t* editor_base;
    menu_number_entry_t* number_entries;
    menu_unit_status_record_t* state;
    s32 portrait_arg;
    const RECT* transition;
    palette_t* portrait_palettes;
    u8* value_pixels;
    u8* name_image;
    menu_status_panel_packet_t* panel_base;
    s16* scroll;
    menu_status_panel_display_thread_t* thread;
    s32 shake;
    s32 dimmed;
    s32 suppress;

    s32 thread_id;
    s32 frame;
    s32 i;
    s32* window_width;
    s32 px1;
    s32 pya;
    s32 px2;
    s32 pyb;
    s32 panel_y_a;
    s32 panel_y_b;
    s32 editor_y_a;
    s32 editor_y_b;
    s16 portrait_cell;
    glyph_blit_t* text_position;
    menu_status_panel_packet_t* panel;
    menu_status_panel_editor_packet_t* editor;
    SPRT* panel_sprites;
    u8* number_pixels_b;
    u8* value_pixels_b;
    POLY_FT4* panel_portrait;

    suppress = 0;
    thread_set_current_task_id(THREAD_TASK_UNIT_STATUS_BANNER);
    g_menu_input_controller = script_get_controller_input_pointer(0) + 1;
    thread_id = g_thread_current_id;
    thread = (menu_status_panel_display_thread_t*)g_threads[thread_id].function_parameter_1;
    if (thread_id == THREAD_SCREEN_SLOT_UNIT_STATUS) {
        panel = g_panel_selected_packets;
        editor = g_panel_selected_editor_packets;
        number_pixels = g_panel_selected_number_image;
        value_pixels = g_panel_selected_large_number_image;
        name_image = g_panel_selected_name_image;
        number_entries = g_panel_selected_numeric_entries;
        state = &g_panel_selected_billboard;
        scroll = g_bunit_panel_selected_unit_data;
        transition = &g_panel_selected_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        portrait_palettes = g_bunit_panel_selected_portrait_palettes;
    } else {
        panel = g_panel_comparison_packets;
        editor = g_panel_comparison_editor_packets;
        number_pixels = g_panel_comparison_number_image;
        value_pixels = g_panel_comparison_large_number_image;
        name_image = g_panel_comparison_name_image;
        number_entries = g_panel_comparison_numeric_entries;
        state = &g_panel_comparison_billboard;
        scroll = g_bunit_panel_comparison_unit_data;
        transition = &g_panel_comparison_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        portrait_palettes = g_bunit_panel_comparison_portrait_palettes;
    }
    shake = 0;

    menu_init_numeric_display_frame_primitives(&g_menu_portrait_origin, &editor->numeric_frame);
    gfx_set_draw_mode_for_texture_page(&editor->draw_mode_a, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    gfx_set_draw_mode_for_texture_page(&editor->draw_mode_b, MENU_TEXTURE_PAGE_FRAME);
    menu_init_sprite_array(&editor->label_sprites[0], 7, 0x7CBC);
    menu_init_sprite_array(&editor->value_sprites[0], 4, 0x7CBC);
    for (frame = 0; frame < 4; frame++) {
        menu_init_default_poly_ft4(&editor->portrait[frame]);
    }
    text_configure_sprite_vram(&rects[0], 0x38, 0x28, &editor->value_sprites[0], 2);
    text_configure_sprite_vram(&rects[1], 0x60, 0x10, &editor->value_sprites[1], 2);
    for (frame = 0; frame < 4; frame++) {
        gfx_init_image_loading(&editor->value_sprites[frame], g_editor_numeric_geometry, &g_menu_portrait_origin,
            &g_panel_editor_value_cells[frame]);
    }
    if (state->team_kind >= 4) {
        state->team_kind = 0;
    }
    if (g_menu_post_battle_unit_changes_active == 0) {
        mem_copy_bytes_nonempty(g_panel_editor_mode_cell, &g_panel_editor_mode_cells[state->team_kind], 0xC);
    } else {
        mem_copy_bytes_nonempty(g_panel_editor_mode_cell, g_panel_editor_mode_cells, 0xC);
    }
    for (frame = 0; frame < 7; frame++) {
        gfx_init_image_loading(&editor->label_sprites[frame], g_editor_numeric_geometry, &g_menu_portrait_origin,
            &g_panel_editor_label_cells[frame]);
    }
    mem_copy_bytes_nonempty(editor + 1, editor, sizeof(menu_status_panel_editor_packet_t));
    gfx_set_draw_mode_for_texture_page(&panel->draw_mode_a, MENU_TEXTURE_PAGE_FRAME);
    gfx_set_draw_mode_for_texture_page(&panel->draw_mode_b, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    menu_init_numeric_display_frame_primitives(&g_panel_frame_rect, &panel->numeric_frame);
    panel_sprites = &panel->sprites[0];
    menu_init_sprite_array(panel_sprites, 7, 0x7C3C);
    menu_init_default_poly_ft4(&panel->portrait);
    text_configure_sprite_vram(&rects[2], 0x58, 0x20, panel_sprites, 0);
    text_configure_sprite_vram(&rects[3], 0x10, 0xA, &panel->sprites[1], 0);
    text_configure_sprite_vram(&rects[4], 0x10, 0xA, &panel->sprites[2], 0);
    text_configure_sprite_vram(&rects[5], 0x10, 0xA, &panel->sprites[3], 0);
    for (frame = 0; frame < 7; frame++) {
        gfx_init_image_loading(
            &panel->sprites[frame], g_editor_numeric_geometry, &g_panel_frame_rect, &g_panel_sprite_cells[frame]);
    }
    gfx_init_image_loading(&panel->portrait, g_editor_numeric_geometry, &g_panel_frame_rect, g_panel_portrait_cell);
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
        if (g_menu_post_battle_unit_changes_active == 0) {
            mem_copy_bytes_nonempty(g_panel_editor_mode_cell, &g_panel_editor_mode_cells[state->team_kind], 0xC);
        } else {
            mem_copy_bytes_nonempty(g_panel_editor_mode_cell, g_panel_editor_mode_cells, 0xC);
        }
        gfx_init_image_loading(
            &editor->label_sprites[6], g_editor_numeric_geometry, &g_menu_portrait_origin, g_panel_editor_mode_cell);
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
            editor->value_sprites[i].clut = g_panel_editor_value_cluts[i][dimmed];
        }
        for (i = 0; i < 7; i++) {
            editor->label_sprites[i].clut = g_panel_editor_label_cluts[i][dimmed];
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
        g_gfx_draw_offset_y = g_frame_arg != 0xF0 ? 0xF0 : 0;
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
                thread->shake_y = g_panel_slide_down_y[shake];
                if (frame & 1) {
                    shake += 1;
                }
                if (shake >= 7) {
                    shake = 0;
                    thread->flags = thread->flags & MENU_STATUS_PANEL_HIDDEN;
                }
            } else if (shake >= 0xB) {
                thread->shake_y = g_panel_slide_up_y[shake];
                if (frame & 1) {
                    shake += 1;
                }
                if (shake >= 0x11) {
                    shake = 0;
                    thread->flags = thread->flags & MENU_STATUS_PANEL_HIDDEN;
                }
            }
        }
        px1 = thread->x;
        panel_y_a = g_gfx_draw_offset_y;
        panel->draw_offset_a.position.x = px1 - 0x80;
        pya = (u16)thread->shake_y;
        panel->draw_offset_a.position.y = pya + panel_y_a;
        SetDrawOffset(&panel->draw_offset_a.packet, (u16*)&panel->draw_offset_a.position);
        panel_y_b = g_gfx_draw_offset_y;
        panel->draw_offset_b.position.x = -0x80;
        panel->draw_offset_b.position.y = panel_y_b;
        SetDrawOffset(&panel->draw_offset_b.packet, (u16*)&panel->draw_offset_b.position);
        px2 = thread->x;
        editor_y_a = g_gfx_draw_offset_y;
        editor->draw_offset_a.position.x = px2 - 0x80;
        pyb = (u16)thread->shake_y;
        editor->draw_offset_a.position.y = pyb + editor_y_a;
        SetDrawOffset(&editor->draw_offset_a.packet, (u16*)&editor->draw_offset_a.position);
        editor_y_b = g_gfx_draw_offset_y;
        editor->draw_offset_b.position.x = -0x80;
        editor->draw_offset_b.position.y = editor_y_b;
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
            transition, &anim_state, &cur_unit, &prev_unit, portrait_palettes, &editor->portrait[0], portrait_arg);
        {
            CVECTOR* color;
            menu_unit_status_gauge_t* bar;

            color = g_panel_gauge_bar_colors;
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
        panel->sprites[6].u0 = (scroll[4] % 7) * 0x18;
        portrait_cell = scroll[4];
        panel->sprites[6].w = 0x18;
        panel->sprites[6].h = 0x14;
        panel->sprites[6].v0 = (portrait_cell / 7) * 0x14 + 0x2A;
        if ((frame == 0) || (thread->work != 0)) {
            if (state->list_index < 0) {
                number_entries[6].format = 0xC00;
            } else {
                number_entries[6].format = 2;
            }
            menu_clear_render_buffer(number_pixels, 0x700);
            window_width = &g_menu_text_state.pen.stride;
            text_position = (glyph_blit_t*)(window_width - 2);
            *window_width = 0x38;
            menu_draw_numeric_display_entries(number_pixels, number_entries, text_position, 6);
            LoadImage(&rects[0], (u32*)number_pixels);
            number_pixels_b = number_pixels + 0x400;
            *window_width = 0x60;
            menu_draw_numeric_display_entries(number_pixels_b, &number_entries[6], text_position, 4);
            LoadImage(&rects[1], (u32*)number_pixels_b);
            thread->work = 0;
            menu_clear_render_buffer(name_image, 0x580);
            *window_width = 0x58;
            menu_set_text_origin(0, 0);
            menu_display_text_entry(scroll[0] + TEXT_ID_UNIT_NAME_BASE, name_image, text_position);
            menu_set_text_origin(0, 0x10);
            menu_display_text_entry(scroll[1] + TEXT_ID_JOB_NAME_BASE, name_image, text_position);
            LoadImage(&rects[2], (u32*)name_image);
            menu_clear_render_buffer(value_pixels, 0xF0);
            *window_width = 0x10;
            menu_set_text_origin(0, 0);
            text_draw_large_number_glyphs_variant(scroll[2], 0x202, value_pixels, text_position);
            LoadImage(&rects[3], (u32*)value_pixels);
            menu_set_text_origin(0, 0);
            value_pixels_b = value_pixels + 0x50;
            text_draw_large_number_glyphs_variant(scroll[3], 0x202, value_pixels_b, text_position);
            LoadImage(&rects[4], (u32*)value_pixels_b);
            if (g_menu_post_battle_unit_changes_active == 0) {
                unit_stats_t* stats;
                menu_set_text_origin(0, 0);
                stats = battle_unit_get_stats_from_battle_id(scroll[5]);
                if (scroll[5] >= 0x10) {
                    s32 id = stats->formation_index;
                    if (id == BATTLE_FORMATION_INDEX_NONE) {
                        id = 0;
                    }
                    text_draw_large_number_glyphs_variant(id + 1, 0x4002, value_pixels + 0xA0, text_position);
                }
            }
            LoadImage(&rects[5], (u32*)(value_pixels + 0xA0));
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
            panel_portrait = &panel->portrait;
            SetSemiTrans(panel_portrait, 1);
            gfx_draw_or_append_gpu_primitive(panel_portrait);
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
        if (thread_get_current_parameter_3() != 0) {
            break;
        }
        frame++;
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
