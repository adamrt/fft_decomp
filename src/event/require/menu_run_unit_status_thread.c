/* The thread drawing the character editor and status panels. */

#include "fft/event_require.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/main_gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_panel.h"
#include "fft/menu_status.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Thread task THREAD_TASK_UNIT_STATUS_BANNER: the character editor and status panels, REQUIRE's
 * body. Same doubled packets (0x30C editor and 0x1D8 panel packets, fft/menu.h) as the other
 * bodies, without the slide animation (the thread's shake_y is used as it stands), with every
 * gauge bar at full width, two numeric entries in the second number image, and the battle id + 1
 * always as the third large number. */
void menu_run_unit_status_thread(void) {
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
    menu_active_unit_t* unit_info;
    menu_status_panel_display_thread_t* thread;
    s32 frame;
    s32 dimmed;
    s32 suppress;

    s32 thread_id;
    s32 i;
    s32* window_width;
    s32 panel_x;
    s32 editor_x;
    s16 zodiac;
    glyph_blit_t* text_position;
    menu_status_panel_packet_t* panel;
    menu_status_panel_editor_packet_t* editor;
    SPRT* panel_sprites;
    u8* number_pixels_b;
    u8* value_pixels_b;
    u8* value_pixels_c;
    POLY_FT4* panel_portrait;

    suppress = 0;
    thread_set_current_task_id(THREAD_TASK_UNIT_STATUS_BANNER);
    g_menu_input_controller = script_get_controller_input_pointer(0) + 1;
    thread_id = g_thread_current_id;
    thread = (menu_status_panel_display_thread_t*)g_threads[thread_id].function_parameter_1;

    /* One body serves two thread slots: the status slot draws the selected unit, the other the comparison unit. */
    if (thread_id == THREAD_SCREEN_SLOT_UNIT_STATUS) {
        panel = g_panel_selected_packets;
        editor = g_panel_selected_editor_packets;
        number_pixels = g_panel_selected_number_image;
        value_pixels = g_panel_selected_large_number_image;
        name_image = g_panel_selected_name_image;
        number_entries = g_panel_selected_numeric_entries;
        state = &g_menu_selected_unit_status;
        unit_info = &g_menu_selected_unit;
        transition = &g_panel_selected_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        portrait_palettes = g_require_panel_selected_portrait_palettes;
    } else {
        panel = g_panel_comparison_packets;
        editor = g_panel_comparison_editor_packets;
        number_pixels = g_panel_comparison_number_image;
        value_pixels = g_panel_comparison_large_number_image;
        name_image = g_panel_comparison_name_image;
        number_entries = g_panel_comparison_numeric_entries;
        state = &g_menu_comparison_unit_status;
        unit_info = &g_menu_comparison_unit_data;
        transition = &g_panel_comparison_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        portrait_palettes = g_require_panel_comparison_portrait_palettes;
    }

    /* Build the editor packets once; the second buffer gets a copy of them below. */
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

    /* Team kinds past 3 fall back to 0: the mode cells have one row per kind (ally, enemy, neutral, auto-battle). */
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

    /* The status panel packets, built and copied the same way. */
    gfx_set_draw_mode_for_texture_page(&panel->draw_mode_a, MENU_TEXTURE_PAGE_FRAME);
    gfx_set_draw_mode_for_texture_page(&panel->draw_mode_b, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    menu_init_numeric_display_frame_primitives(&g_panel_frame_rect, &panel->numeric_frame);
    panel_sprites = &panel->sprites[0];
    menu_init_sprite_array(panel_sprites, 7, MENU_CLUT_NORMAL);
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

    /* Both portrait slots start on the current unit, so no transition runs until it changes. */
    frame = 0;
    anim_state = 0;
    prev_unit = state->unit_index;
    cur_unit = state->unit_index;
    for (;;) {
        /* Alternate packet copies each frame so the GPU reads one while the other is rebuilt. */
        s32 parity = frame & 1;
        panel = panel_base + parity;
        editor = editor_base + parity;

        /* CT can run past the turn threshold; clamp it so its gauge never shows more than full. */
        if (state->gauges[MENU_UNIT_STATUS_GAUGE_CT].value > UNIT_TURN_CT_THRESHOLD) {
            state->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = UNIT_TURN_CT_THRESHOLD;
        }

        /* The team kind can change while the panel is up: refresh the mode row and portrait CLUT every frame. */
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

        /* Dim while a THREAD_TASK_STATUS_PANEL thread runs under an input lock, or on the thread's request. */
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
                panel->sprites[i].clut = MENU_CLUT_DIMMED;
            } else {
                panel->sprites[i].clut = MENU_CLUT_NORMAL;
            }
            i += 1;
        } while (i < 7);

        if (dimmed != 0) {
            menu_init_primitive_colors_dimmed(&editor->numeric_frame);
            menu_init_primitive_colors_dimmed(&panel->numeric_frame);
            /* Unsigned test for transition steps 4-9: during the wipe quad 0 takes the shade of the other three. */
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

        /* Offset a moves each block to the thread's x and shake_y; offset b restores the plain offset after it. */
        panel_x = thread->x;
        panel->draw_offset_a.position.x = panel_x - 0x80;
        /* Offset y picks the frame-buffer half (0 or 0xF0) that this frame draws into. */
        g_gfx_draw_offset_y = ((g_main_gfx_screen_polarity * 0xF0) != 0xF0) ? 0xF0 : 0;
        panel->draw_offset_a.position.y = thread->shake_y + g_gfx_draw_offset_y;
        SetDrawOffset(&panel->draw_offset_a.packet, (u16*)&panel->draw_offset_a.position);
        panel->draw_offset_b.position.x = -0x80;
        panel->draw_offset_b.position.y = g_gfx_draw_offset_y;
        SetDrawOffset(&panel->draw_offset_b.packet, (u16*)&panel->draw_offset_b.position);

        editor_x = thread->x;
        editor->draw_offset_a.position.x = editor_x - 0x80;
        editor->draw_offset_a.position.y = thread->shake_y + g_gfx_draw_offset_y;
        SetDrawOffset(&editor->draw_offset_a.packet, (u16*)&editor->draw_offset_a.position);
        editor->draw_offset_b.position.x = -0x80;
        editor->draw_offset_b.position.y = g_gfx_draw_offset_y;
        SetDrawOffset(&editor->draw_offset_b.packet, (u16*)&editor->draw_offset_b.position);

        /* Transition steps: 0 still portrait, 2-3 CLUT capture, 4-9 wipe. Past the wipe, return to a still portrait;
         * suppress latches the hidden flag here, keeping the portrait undrawn until a later wipe ends while visible. */
        if (anim_state >= 0xA) {
            anim_state = 0;
            prev_unit = cur_unit;
            suppress = thread->flags & MENU_STATUS_PANEL_HIDDEN;
        }
        /* thread->work is both the redraw request and the wipe direction; a request mid-wipe restarts it at step 2. */
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

        /* Gauge bars (HP, MP, CT) as Gouraud quads with four vertex colors each.
         * This copy draws every bar at its full 32 pixels. */
        {
            CVECTOR* color;

            color = g_panel_gauge_bar_colors;
            for (i = 0; i < 3; i++) {
                s32 x0;
                s32 x1;
                s32 y;

                SetPolyG4(&editor->bars[i]);
                x1 = g_menu_portrait_origin.x;
                x0 = x1 + 0x2F;
                x1 += 0x4F;
                editor->bars[i].x0 = x0;
                y = i * 0xB + 0x18;
                editor->bars[i].y0 = g_menu_portrait_origin.y + y;
                editor->bars[i].x1 = x1;
                editor->bars[i].y1 = g_menu_portrait_origin.y + y;
                editor->bars[i].x2 = x0;
                editor->bars[i].y2 = g_menu_portrait_origin.y + y + 3;
                editor->bars[i].x3 = x1;
                editor->bars[i].y3 = g_menu_portrait_origin.y + y + 3;
                /* Shift the bottom edge 3 pixels left to slant the bar. */
                editor->bars[i].x2 -= 3;
                editor->bars[i].x3 -= 3;

                /* Dimmed bars halve red and green but keep blue, like the bluish dimmed portrait shades. */
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

        /* The zodiac sign cell: 24x20 cells in rows of seven, starting at v 0x2A. */
        panel->sprites[6].u0 = (unit_info->zodiac % 7) * 0x18;
        zodiac = unit_info->zodiac;
        panel->sprites[6].w = 0x18;
        panel->sprites[6].h = 0x14;
        panel->sprites[6].v0 = (zodiac / 7) * 0x14 + 0x2A;

        /* Rebuild the text images only on the first frame and on redraw requests; VRAM keeps them in between. */
        if ((frame == 0) || (thread->work != 0)) {
            /* Units off the AT list (negative index) show dashes. */
            if (state->list_index < 0) {
                number_entries[6].format = 0xC00;
            } else {
                number_entries[6].format = 2;
            }
            menu_clear_render_buffer(number_pixels, 0x700);
            window_width = &g_menu_text_state.pen.stride;
            /* The pen record begins two words before its stride member. */
            text_position = (glyph_blit_t*)(window_width - 2);
            *window_width = 0x38;
            menu_draw_numeric_display_entries(number_pixels, number_entries, text_position, 6);
            LoadImage(&rects[0], (u32*)number_pixels);

            number_pixels_b = number_pixels + 0x400;
            *window_width = 0x60;
            menu_draw_numeric_display_entries(number_pixels_b, &number_entries[8], text_position, 2);
            LoadImage(&rects[1], (u32*)number_pixels_b);
            thread->work = 0;

            menu_clear_render_buffer(name_image, 0x580);
            *window_width = 0x58;
            menu_set_text_origin(0, 0);
            menu_display_text_entry(unit_info->name_id + TEXT_ID_UNIT_NAME_BASE, name_image, text_position);
            menu_set_text_origin(0, 0x10);
            menu_display_text_entry(unit_info->job_id + TEXT_ID_JOB_NAME_BASE, name_image, text_position);
            LoadImage(&rects[2], (u32*)name_image);

            menu_clear_render_buffer(value_pixels, 0xF0);
            *window_width = 0x10;
            menu_set_text_origin(0, 0);
            text_draw_large_number_glyphs(unit_info->brave, 0x202, value_pixels, text_position);
            LoadImage(&rects[3], (u32*)value_pixels);
            menu_set_text_origin(0, 0);
            value_pixels_b = value_pixels + 0x50;
            text_draw_large_number_glyphs(unit_info->faith, 0x202, value_pixels_b, text_position);
            LoadImage(&rects[4], (u32*)value_pixels_b);

            /* Post-battle changes leave the third cell blank: its buffer was cleared above. */
            if (g_menu_post_battle_unit_changes_active == 0) {
                menu_set_text_origin(0, 0);
                value_pixels_c = value_pixels + 0xA0;
                text_draw_large_number_glyphs(unit_info->battle_id + 1, 0x4002, value_pixels_c, text_position);
                LoadImage(&rects[5], (u32*)value_pixels_c);
            } else {
                LoadImage(&rects[5], (u32*)(value_pixels + 0xA0));
            }
        }

        /* addPrim links each packet at the head of the ordering-table entry, so the GPU runs this list bottom-up:
         * each draw offset and draw mode applies to the packets appended before it. */
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
                /* Steps 5 and up also draw the wipe's other three quads. */
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
