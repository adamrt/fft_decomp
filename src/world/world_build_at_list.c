/* The target's lone dead `lw t1,0xe0(sp)` (reload of the spilled `layout`)
 * at +0x4d4 comes from a conditional on `layout` whose body is a dead store: flow deletes
 * the store, the now-empty branch survives until after reload, and the post-reload jump
 * pass deletes the branch but not its operand reload. */
#include "fft/data.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

void world_build_at_list(void) {
    world_menu_text_window_t window;
    RECT image_rect;
    RECT clear;
    RECT texture_window;
    SPRT icons0[2];
    SPRT icons1[2];
    SPRT icons2[2];
    s32 row_offset;
    s32 ring;
    s32 cursor;
    u32* input0;
    u32* input1;
    SPRT* frame;
    world_menu_list_page_t* page;
    world_menu_text_layout_t* layout;
    s32 accum;
    s32 timer;
    s32 redraw_pending;
    world_menu_entry_t* entry;
    s32 delta;
    s32 i;
    s32 column;
    s32 index;
    s32 arrow_u;
    s32 step;
    s32 top;
    s32 repeat;
    u8* buffer;
    s32 arrow_y;
    s32 span;

    page = g_world_at_list_pages;
    timer = 0;
    input0 = world_input_get_menu_controller(0);
    input1 = world_input_get_menu_controller(1);
    row_offset = 0;
    cursor = 0;
    entry = world_thread_get_current_parameter_1();
    layout = (world_menu_text_layout_t*)entry->text_binding;
    entry->window_height = layout->row_count * 16 + 0x1C;
    entry->inner_height = layout->row_count * 16 + 0x1C;
    entry->overall_height = layout->row_count * 16 + 0x1C;
    window.rect.x = 0;
    window.rect.y = 0x7E;
    window.x_screen_offset = 8;
    window.y_screen_offset = 0xC;
    window.rect.w = entry->window_width - 0x12;
    window.rect.h = entry->window_height - 0x1A;
    world_gfx_set_image_draw_mode(&page->icon_mode, 0);
    texture_window.x = 0;
    texture_window.y = (u8)entry->vram_y;
    texture_window.w = 0x100;
    texture_window.h = 0x100;
    for (i = 0; i < 2; i++) {
        g_world_at_list_pages[i].icons.icons[0] = &icons0[i];
        g_world_at_list_pages[i].icons.icons[1] = &icons1[i];
        g_world_at_list_pages[i].icons.icons[2] = &icons2[i];
        world_menu_build_icon_record(
            &image_rect, (world_menu_icon_thread_param_t*)entry, &g_world_at_list_pages[i].icons);
    }
    world_gfx_reset_record_texture_window(&page->draw_areas);
    SetDrawMode(
        &page->text_mode, 1, 0, GetTPage(0, 0, entry->vram_x & 0xFFC0, entry->vram_y & 0xFF00), &texture_window);
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
    buffer = g_world_menu_text_page_image;
    world_clear_menu_render_buffer(buffer, entry->inner_width * entry->inner_height / 2);
    redraw_pending = 1;
    world_thread_start(0xF, world_menu_window_frame_thread);
    world_thread_set_parameters(0xF, (s32)&entry->window_x, 0, 0);
    row_offset = entry->select_text_table;
    cursor = entry->selected_index - row_offset;
    world_menu_draw_text_columns_2(entry, &row_offset, buffer);
    ring = 0;
    image_rect.w = g_world_menu_text_state.stride >> 2;
    LoadImage(&image_rect, (u32*)buffer);
    clear.h = 2;
    clear.x = image_rect.x;
    clear.y = ((image_rect.y - 2) & 0xFF) + (image_rect.y & 0xFF00);
    clear.w = image_rect.w;
    ClearImage(&clear, 0, 0, 0);
    world_script_copy_bytes(&g_world_at_list_pages[1], g_world_at_list_pages, sizeof(world_menu_list_page_t));
    for (i = 0; i < 2; i++) {
        g_world_at_list_pages[i].icons.icons[0] = &icons0[i];
        g_world_at_list_pages[i].icons.icons[1] = &icons1[i];
        g_world_at_list_pages[i].icons.icons[2] = &icons2[i];
    }
    delta = 0;
    accum = 0;
    *input0 = 0;
    i = 0;
    *input1 = 0;
    for (;;) {
        page = &g_world_at_list_pages[i & 1];
        frame = &page->frame;
        if (world_thread_is_previous_running() != 0 && g_world_thread_task_active == 0) {
            world_thread_set_parameters(0xF, (s32)&entry->window_x, 1, 0);
        } else {
            world_thread_set_parameters(0xF, (s32)&entry->window_x, 0, 0);
        }
        if (delta == 0 && world_menu_is_input_allowed() != 0 && i != 0) {
            /* Dead store (column is reinitialised before use); only the reload
             * of layout for this branch survives in the target. */
            if (layout == 0) {
                column = 0;
            }
            if ((*input0 & PSX_PAD_UP) && !(*input1 & PSX_PAD_SQUARE)) {
                if (cursor != 0) {
                    cursor--;
                    g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset != 0) {
                    delta = -1;
                    g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                } else if (layout->hidden_rows == 0) {
                    cursor = layout->row_count - 1;
                    if (layout->row_count != 1) {
                        g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                    }
                }
            } else if ((*input0 & PSX_PAD_DOWN) && !(*input1 & PSX_PAD_SQUARE)) {
                if (cursor != layout->row_count - 1) {
                    cursor++;
                    g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset != layout->hidden_rows) {
                    delta++;
                    g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset == 0) {
                    cursor = 0;
                    if (layout->row_count != 1) {
                        g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                    }
                }
            } else {
                timer = 0;
            }
            if (delta != 0) {
                if (delta > 0) {
                    image_rect.y = (((ring + layout->row_count) * 16) & 0xFF) + entry->vram_y;
                } else {
                    image_rect.y = (((ring - 1) * 16) & 0xFF) + entry->vram_y;
                }
                image_rect.h = 0x10;
                buffer = g_world_menu_text_page_image;
                world_clear_menu_render_buffer(buffer, 0x800);
                g_world_menu_text_state.stride = entry->inner_width;
                for (column = 0; column < 3; column++) {
                    index = cursor + row_offset + delta;
                    world_menu_set_text_origin(layout->x[column], 0);
                    if (layout->mode[column] != 2) {
                        g_world_menu_text_color = layout->text_colors[column][index];
                    }
                    if (g_world_menu_text_color == 4) {
                        g_world_menu_text_state.color = 0x44444444;
                    }
                    if (layout->mode[column] == 0) {
                        world_menu_display_text_entry(
                            layout->text_ids[column][index], buffer, &g_world_menu_text_state.origin_x);
                    } else if (layout->mode[column] == 1) {
                        world_display_menu_number_entry(
                            layout->text_ids[column][index], 2, (s32)buffer, (u16*)&g_world_menu_text_state.origin_x);
                    } else if (layout->mode[column] == 3) {
                        world_draw_menu_number_glyphs(layout->text_ids[column][index], 2, buffer,
                            (world_glyph_blit_t*)&g_world_menu_text_state.origin_x);
                    }
                }
                g_world_menu_text_state.color = 0;
                g_world_menu_text_color = 0;
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
            redraw_pending = 0;
        }
        if (delta == 0 && world_menu_is_input_allowed() != 0) {
            redraw_pending = (s32)((u8 * (*)(world_menu_entry_t*, s32*, s32*, s32*))
                    world_menu_redraw_text_page_on_scroll)(entry, &row_offset, &ring, &cursor);
            ClearImage(&clear, 0, 0, 0);
        }
        entry->selected_index = cursor + row_offset;
        entry->select_text_table = row_offset;
        if (world_menu_check_thread_completion((s32*)input0) != 0) {
            break;
        }
        if (delta == 0) {
            world_menu_handle_entry_confirm(entry, cursor);
        }
        world_menu_cancel_thread_group((world_menu_cancel_context_t*)entry);
        if ((g_world_event_speed == 1 && !(world_input_get_menu_repeat_counter() & 1)) || g_world_event_speed == 2) {
            if (timer < g_main_menu_scroll_accel_delay) {
                step = g_main_menu_scroll_slow_step << 1;
            } else {
                step = g_main_menu_scroll_fast_step << 1;
            }
            accum += delta * step;
        }
        timer += g_world_event_speed;
        if ((u32)(accum + 0xF) >= 0x1F) {
            ring += delta;
            row_offset += delta;
            repeat = world_input_get_menu_repeat_counter();
            delta = 0;
            accum = 0;
            if (repeat >= g_main_input_repeat_initial_delay / g_world_event_speed) {
                *input0 |= *input1 & (PSX_PAD_UP | PSX_PAD_DOWN);
            }
        }
        frame->v0 = ring * 16 + accum - 2;
        if (world_thread_is_previous_running() != 0 || g_world_thread_task_active == 1) {
            frame->clut = 0x7F7D;
        } else {
            frame->clut = 0x7FFC;
        }
        if (layout->hidden_rows != 0 && world_menu_is_input_allowed() != 0) {
            if (row_offset != 0) {
                world_gfx_draw_or_append_gpu_primitive(&page->arrows[0]);
                world_gfx_draw_or_append_gpu_primitive(&page->arrow_marks[0]);
            }
            if (row_offset != layout->hidden_rows) {
                world_gfx_draw_or_append_gpu_primitive(&page->arrows[1]);
                world_gfx_draw_or_append_gpu_primitive(&page->arrow_marks[1]);
            }
            span = page->arrows[1].y0;
            top = page->arrows[0].y0 + page->arrows[0].h;
            span -= top;
            page->thumb.x0 = page->arrows[0].x0;
            page->thumb.y0 = top + span * entry->selected_index / (layout->row_count + layout->hidden_rows) - 2;
            world_gfx_draw_or_append_gpu_primitive(&page->thumb);
        }
        world_gfx_build_scaled_draw_area_pair_swapped(
            &page->draw_areas, &entry->window_x, i, g_main_gfx_display_buffer_index);
        world_menu_select_task_icon_tile_2(page->icons.icons[0]);
        if ((s16)entry->header_id >= 5) {
            world_gfx_draw_or_append_gpu_primitive(page->icons.icons[0]);
        }
        world_gfx_draw_or_append_gpu_primitive(&page->draw_areas.areas[1]);
        world_gfx_draw_or_append_gpu_primitive(&page->icon_mode);
        world_gfx_draw_or_append_gpu_primitive(frame);
        world_gfx_draw_or_append_gpu_primitive(&page->text_mode);
        world_gfx_draw_or_append_gpu_primitive(&page->draw_areas);
        world_menu_select_icon_cluts(&page->icons.base);
        world_menu_update_icon_cursor_sprites((world_menu_icon_thread_param_t*)entry, &page->icons.base, i, cursor);
        i++;
        page->icons.base.sprites[0].x0 = 1000;
        page->icons.base.sprites[1].y0 += 4;
        page->icons.base.sprites[2].y0 += 4;
        world_menu_submit_icon_primitives(&page->icons.base);
    }
    world_thread_set_parameters(0xF, 0, 0, 1);
    entry->select_text_table = row_offset;
    world_thread_yield();
    world_thread_exit_current();
}
