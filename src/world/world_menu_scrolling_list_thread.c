#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Provisional: three-column layout with the saved first visible row. */

/*
 * Scrolling three-column ability list thread (the WORLD twin of BATTLE
 * battle_menu_run_scrolling_ability_list_thread, reached from world_menu_preview_ability_turn_at_list).
 *
 * Draws the visible rows into a text image, then runs the input loop on two
 * alternating packet pages. Cursor moves past either end start a one-row
 * scroll whose new row is drawn into the image ring at row `ring * 16`;
 * world_menu_redraw_text_page_on_scroll_in_rect handles page scrolling, and
 * the scroll-bar thumb tracks entry->selected_index. The frame sprite pointer is
 * passed to SetSemiTrans by address, as in the target, which keeps it on the
 * stack.
 */
void world_menu_scrolling_list_thread(void) {
    world_menu_text_window_t window;
    RECT image_rect;
    RECT texture_window;
    SPRT icons0[2];
    SPRT icons1[2];
    SPRT icons2[2];
    RECT clear;
    s32 row_offset;
    SPRT* frame;
    s16 toggle;
    s32 ring;
    s32 cursor;
    world_menu_list_page_t* base;
    world_menu_scroll_text_layout_t* layout;
    s32 accum;
    s32 delta;
    s32 timer;
    s32 redraw_pending;
    s32 row_buffer_live;
    world_menu_entry_t* entry;
    world_menu_list_page_t* page;
    u8* buffer;
    s32 i;
    s32 column;
    s32 index;
    s32 value;
    s32 step;
    s32 top;
    s32 arrow_u;
    s32 arrow_y;
    s32 repeat;
    s32 span;

    toggle = 0;
    page = g_world_ability_list_pages[g_world_thread_current_id];
    entry = world_thread_get_current_parameter_1();
    layout = (world_menu_scroll_text_layout_t*)entry->text_binding;
    entry->window_height = layout->columns.row_count * 16 + 0x1C;
    entry->inner_height = layout->columns.row_count * 16 + 0x1C;
    row_buffer_live = 0;
    entry->overall_height = layout->columns.row_count * 16 + 0x1C;
    window.rect.x = 0x108 - entry->inner_width;
    window.rect.y = 0xFE;
    window.x_screen_offset = 8;
    window.y_screen_offset = 0xC;
    timer = 0;
    window.rect.w = entry->inner_width - 8;
    window.rect.h = entry->window_height - 0x1A;
    base = page;
    clear.x = window.rect.x / 4 + entry->vram_x;
    clear.y = (entry->vram_y & 0xFF00) + 0xFE;
    clear.w = window.rect.w / 4;
    clear.h = 2;
    ClearImage(&clear, 0, 0, 0);
    texture_window.x = 0;
    texture_window.y = 0x80;
    texture_window.w = 0x100;
    texture_window.h = 0x80;
    for (i = 0; i < 2; i++) {
        page[i].icons.icons[0] = &icons0[i];
        page[i].icons.icons[1] = &icons1[i];
        page[i].icons.icons[2] = &icons2[i];
        world_menu_build_icon_record(&image_rect, (world_menu_icon_thread_param_t*)entry, &page[i].icons);
    }
    SetDrawMode(&page->icon_mode, 0, 0, GetTPage(0, 2, 0x3C0, 0x100), &g_world_gfx_texture_window);
    SetDrawMode(
        &page->text_mode, 1, 0, GetTPage(0, 1, entry->vram_x & 0xFFC0, entry->vram_y & 0xFF00), &texture_window);
    for (i = 0, arrow_u = 0xD8; i < 2; i++) {
        world_menu_init_sprite(&page->arrows[i]);
        page->arrows[i].clut = 0x7D7C;
        page->arrows[i].w = 8;
        page->arrows[i].h = 0x10;
        page->arrows[i].u0 = arrow_u;
        page->arrows[i].v0 = 0;
        page->arrows[i].x0 = entry->window_x + entry->window_width - 0xF;
        arrow_y = i * (entry->window_height - 0x32) + 0x10;
        page->arrows[i].y0 = entry->window_y + arrow_y;
        arrow_u += 8;
    }
    world_menu_init_sprite(&page->thumb);
    page->thumb.u0 = 0x10;
    page->thumb.v0 = 0x10;
    page->thumb.w = 8;
    page->thumb.h = 8;
    page->thumb.clut = 0x7D7C;
    world_menu_init_sprite(&page->frame);
    world_gfx_init_image_loading((POLY_FT4*)&page->frame, (const world_image_location_t*)entry,
        (const world_image_location_t*)&entry->window_x, (const world_gfx_image_load_parameters_t*)&window);
    world_menu_init_sprite_array(page->arrow_marks, 2, 0x7C3C);
    page->arrow_marks[0].w = 6;
    page->arrow_marks[0].h = 0xC;
    page->arrow_marks[1].w = 4;
    page->arrow_marks[1].h = 4;
    page->arrow_marks[0].u0 = 0xE8;
    page->arrow_marks[1].u0 = 0xE8;
    page->arrow_marks[0].v0 = 0;
    page->arrow_marks[0].x0 = page->arrows[0].x0 - 1;
    page->arrow_marks[0].y0 = page->arrows[0].y0 + 3;
    page->arrow_marks[1].v0 = 0xC;
    page->arrow_marks[1].x0 = page->arrows[1].x0;
    page->arrow_marks[1].y0 = page->arrows[1].y0 + 0xE;
    world_menu_build_and_upload_window_frame_image(entry->inner_width, entry->inner_height, &image_rect, 2);
    buffer = world_menu_alloc_ui_buffer(entry->inner_width * entry->inner_height / 2);
    world_clear_menu_render_buffer(buffer, window.rect.w * window.rect.h / 2);
    row_offset = layout->row_offset;
    if (layout->columns.hidden_rows < row_offset) {
        row_offset = layout->columns.hidden_rows;
    }
    cursor = entry->selected_index - row_offset;
    if (cursor >= layout->columns.row_count) {
        row_offset = 0;
        cursor = 0;
    }
    world_draw_menu_text_columns_narrow(entry, &row_offset, buffer);
    redraw_pending = 1;
    ring = 0;
    image_rect.x = window.rect.x / 4 + entry->vram_x;
    image_rect.y = 0x80;
    image_rect.w = window.rect.w / 4;
    image_rect.h = window.rect.h;
    LoadImage(&image_rect, (u32*)buffer);
    world_script_copy_bytes(&base[1], base, sizeof(world_menu_list_page_t));
    for (i = 0; i < 2; i++) {
        base[i].icons.icons[0] = &icons0[i];
        base[i].icons.icons[1] = &icons1[i];
        base[i].icons.icons[2] = &icons2[i];
    }
    i = 0;
    accum = 0;
    delta = 0;
    for (;;) {
        page = &base[i & 1];
        frame = &page->frame;
        SetSemiTrans(&frame, 1);
        if (delta == 0 && world_menu_is_input_allowed() != 0 && i != 0) {
            if (layout->columns.mode[0] == 0 && layout->columns.mode[1] == 0) {
                if (g_world_menu_new_button_input & (PSX_PAD_RIGHT | PSX_PAD_LEFT)) {
                    toggle ^= 1;
                }
                layout->columns.text_ids[2] = &toggle;
            }
            if ((g_world_menu_new_button_input & PSX_PAD_UP)
                && (!(g_world_menu_current_button_input & PSX_PAD_SQUARE) || g_world_menu_input_disabled != 0)) {
                if (cursor != 0) {
                    cursor--;
                    world_sound_set_scroll_effect();
                } else if (row_offset != 0) {
                    delta = -1;
                    world_sound_set_scroll_effect();
                } else if (layout->columns.hidden_rows == 0) {
                    cursor = layout->columns.row_count - 1;
                    world_sound_set_scroll_effect();
                }
            } else if ((g_world_menu_new_button_input & PSX_PAD_DOWN)
                && (!(g_world_menu_current_button_input & PSX_PAD_SQUARE) || g_world_menu_input_disabled != 0)) {
                if (cursor != layout->columns.row_count - 1) {
                    cursor++;
                    world_sound_set_scroll_effect();
                } else if (row_offset != layout->columns.hidden_rows) {
                    delta++;
                    world_sound_set_scroll_effect();
                } else if (row_offset == 0) {
                    cursor = 0;
                    world_sound_set_scroll_effect();
                }
            } else {
                timer = 0;
            }
            if (delta != 0) {
                if (delta > 0) {
                    image_rect.y = (((ring + layout->columns.row_count) * 16) & 0x7F) + entry->vram_y;
                } else {
                    image_rect.y = (((ring - 1) * 16) & 0x7F) + entry->vram_y;
                }
                image_rect.h = 0x10;
                image_rect.y += 0x80;
                row_buffer_live = 1;
                buffer = world_menu_alloc_ui_buffer(window.rect.w * 8);
                world_clear_menu_render_buffer(buffer, window.rect.w * 8);
                g_world_menu_text_state.stride = window.rect.w;
                for (column = 0; column < 3; column++) {
                    index = cursor + row_offset + delta;
                    world_menu_set_text_origin(layout->columns.x[column], 0);
                    if (layout->columns.mode[column] != 2) {
                        g_world_menu_text_color = layout->columns.text_colors[column][index];
                    }
                    g_world_menu_text_state.color = 0;
                    if (g_world_menu_text_color == 4) {
                        g_world_menu_text_state.color = 0x44444444;
                    }
                    if (layout->columns.mode[column] == 0) {
                        world_menu_display_text_entry(
                            layout->columns.text_ids[column][index], buffer, &g_world_menu_text_state.origin_x);
                        g_world_menu_text_color = layout->columns.text_colors[column][index];
                        if (g_world_thread_contexts[g_world_thread_current_id].function_parameter_4 != 0
                            && (((world_ability_skill_use_tables_t*)g_battle_ai_workspace_ptr)->flags[index] & 1)) {
                            /* The target passes both coordinates without the s16 sign extension. */
                            ((void (*)(s32, s32))world_menu_set_text_origin)(layout->columns.x[column] + 0x50, 0);
                            world_menu_display_text_entry(0x5088, buffer, &g_world_menu_text_state.origin_x);
                        }
                    } else if (layout->columns.mode[column] == 1) {
                        world_display_menu_number_entry(layout->columns.text_ids[column][index], 2, (s32)buffer,
                            (u16*)&g_world_menu_text_state.origin_x);
                    } else if (layout->columns.mode[column] == 3) {
                        if ((column == 2 || (column == 1 && layout->columns.mode[2] == 2))
                            && (u16)layout->columns.text_ids[column][index] < 2
                            && (((world_ability_skill_use_tables_t*)g_battle_ai_workspace_ptr)->target[index] == 0xFF
                                || ((world_ability_skill_use_tables_t*)g_battle_ai_workspace_ptr)->target[index]
                                    == 0)) {
                            g_world_menu_text_state.origin_y += 2;
                            world_text_draw_shadowed(buffer, (world_glyph_blit_t*)&g_world_menu_text_state.origin_x);
                        } else {
                            value = (u16)layout->columns.text_ids[column][index];
                            if (value & 0x7FF) {
                                if ((value & 0x7FF) >= 100) {
                                    g_world_menu_text_color = 0;
                                    g_world_menu_text_state.origin_x += 3;
                                    world_menu_display_text_entry(0x509D, buffer, &g_world_menu_text_state.origin_x);
                                } else {
                                    world_draw_menu_number_glyphs(
                                        (s16)value, 2, buffer, (world_glyph_blit_t*)&g_world_menu_text_state.origin_x);
                                }
                            }
                        }
                    }
                }
                g_world_menu_text_color = 0;
                g_world_menu_text_state.color = 0;
                LoadImage(&image_rect, (u32*)buffer);
                clear.h = 2;
                clear.x = image_rect.x;
                clear.y = ((image_rect.y - 2) & 0xFF) + (image_rect.y & 0xFF00);
                clear.w = image_rect.w;
                ClearImage(&clear, 0, 0, 0);
            }
        }
        world_thread_yield();
        if (redraw_pending != 0) {
            world_menu_free_memory(buffer);
            redraw_pending = 0;
        }
        if (delta == 0 && world_menu_is_input_allowed() != 0) {
            buffer = world_menu_redraw_text_page_on_scroll_in_rect(entry, &row_offset, &ring, &cursor, &window.rect);
            redraw_pending = (s32)buffer;
            ClearImage(&clear, 0, 0, 0);
        }
        entry->selected_index = cursor + row_offset;
        layout->row_offset = row_offset;
        if (world_menu_check_thread_completion(&g_world_menu_new_button_input) != 0) {
            break;
        }
        world_menu_select_icon_cluts(&page->icons.base);
        world_menu_select_task_icon_tile_2(page->icons.icons[0]);
        world_menu_select_task_icon_tile_2(page->icons.icons[1]);
        world_menu_select_task_icon_tile_2(page->icons.icons[2]);
        if (world_thread_is_previous_running() != 0 || g_world_thread_task_active == 1) {
            frame->clut = 0x7F7D;
        } else {
            frame->clut = 0x7FFC;
        }
        world_menu_update_icon_cursor_sprites((world_menu_icon_thread_param_t*)entry, &page->icons.base, i, cursor);
        if (layout->columns.hidden_rows != 0 && world_thread_is_previous_running() == 0) {
            if (row_offset != 0) {
                world_gfx_draw_or_append_gpu_primitive(&page->arrows[0]);
                world_gfx_draw_or_append_gpu_primitive(&page->arrow_marks[0]);
            }
            if (row_offset != layout->columns.hidden_rows) {
                world_gfx_draw_or_append_gpu_primitive(&page->arrows[1]);
                world_gfx_draw_or_append_gpu_primitive(&page->arrow_marks[1]);
            }
            span = page->arrows[1].y0;
            top = page->arrows[0].y0 + page->arrows[0].h;
            span -= top;
            page->thumb.x0 = page->arrows[0].x0;
            page->thumb.y0
                = top + span * entry->selected_index / (layout->columns.row_count + layout->columns.hidden_rows) - 2;
            world_gfx_draw_or_append_gpu_primitive(&page->thumb);
        }
        if (g_world_menu_scrolling_list_depth < 2 && delta == 0) {
            world_menu_handle_entry_confirm(entry, cursor);
        }
        world_menu_cancel_thread_group((world_menu_cancel_context_t*)entry);
        if ((g_world_event_speed == 1 && !(world_input_get_menu_repeat_counter() & 1)) || g_world_event_speed == 2) {
            if (timer < g_main_menu_scroll_accel_delay) {
                step = g_main_menu_scroll_slow_step;
            } else {
                step = g_main_menu_scroll_fast_step;
            }
            accum += delta * (step << 1);
        }
        timer += g_world_event_speed;
        if ((u32)(accum + 0xF) >= 0x1F) {
            ring += delta;
            row_offset += delta;
            repeat = world_input_get_menu_repeat_counter();
            accum = 0;
            delta = 0;
            if (repeat >= g_main_input_repeat_initial_delay / g_world_event_speed) {
                g_world_menu_new_button_input |= g_world_menu_current_button_input & (PSX_PAD_UP | PSX_PAD_DOWN);
            }
            world_menu_free_memory(buffer);
            row_buffer_live = 0;
        }
        frame->v0 = (ring * 16 + accum - 2) & 0x7F;
        if (i * g_world_event_speed < 12) {
            world_gfx_init_image_loading((POLY_FT4*)page, (const world_image_location_t*)entry,
                (const world_image_location_t*)&entry->window_x, (const world_gfx_image_load_parameters_t*)&window);
            world_menu_zoom_cursor_frame(entry, frame, i * g_world_event_speed);
        }
        if (layout->columns.mode[1] == 0 && toggle == 1) {
            page->icons.base.sprites[1].x0 += ((world_menu_text_layout_t*)entry->text_binding)->x[1];
            page->icons.base.sprites[2].x0 += ((world_menu_text_layout_t*)entry->text_binding)->x[1];
        }
        page->icons.base.sprites[1].y0 += 4;
        page->icons.base.sprites[2].y0 += 4;
        if ((s16)entry->header_id >= 5) {
            world_gfx_draw_or_append_gpu_primitive(page->icons.icons[0]);
            world_gfx_draw_or_append_gpu_primitive(page->icons.icons[1]);
            if (g_world_thread_contexts[g_world_thread_current_id].function_parameter_4 != 0) {
                world_gfx_draw_or_append_gpu_primitive(page->icons.icons[2]);
            }
        }
        world_gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[3]);
        world_gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[1]);
        world_gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[2]);
        world_gfx_draw_or_append_gpu_primitive(&page->icon_mode);
        world_gfx_draw_or_append_gpu_primitive(frame);
        world_gfx_draw_or_append_gpu_primitive(&page->text_mode);
        world_gfx_draw_or_append_gpu_primitive(&page->icons.base.draw_mode);
        world_gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[0]);
        world_gfx_draw_or_append_gpu_primitive(&page->icons.base);
        i++;
    }
    if (row_buffer_live != 0) {
        world_menu_free_memory(buffer);
    }
    world_thread_yield();
    g_world_menu_scrolling_list_depth--;
    if (world_thread_get_current_parameter_2() == 0) {
        world_thread_exit_current();
    }
}
