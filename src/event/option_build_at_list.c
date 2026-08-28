/* Build the AT list.
 *
 * Port of world_build_at_list. The do/while (0) around the scroll redraw puts its render-buffer
 * references one loop level deeper, which raises the buffer's global-alloc priority above the
 * page pointer's (target: buffer s3, page s4; without it the two swap). */
#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_menu_window.h"
#include "fft/battle_text.h"
#include "fft/data.h"
#include "fft/event.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/menu_types.h"
#include "fft/option.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

struct menu_frame_sprites;
struct battle_menu_frame_primitives;

void battle_gfx_init_image_loading(POLY_FT4* primitive, const battle_image_location_t* base_load,
    const battle_image_location_t* base_screen, const world_gfx_image_load_parameters_t* parameters);

/* OPTION's pair of list pages, alternated per frame. */
extern world_menu_list_page_t g_option_at_list_pages[2];

void option_build_at_list(void) {
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
    u32* input;
    u32* input2;
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

    page = g_option_at_list_pages;
    timer = 0;
    input = battle_script_get_controller_input_pointer(0);
    input2 = battle_script_get_controller_input_pointer(1);
    row_offset = 0;
    cursor = 0;
    entry = (world_menu_entry_t*)battle_thread_get_current_parameter_1();
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
    battle_gfx_set_draw_mode_for_texture_page(&page->icon_mode, 0);
    texture_window.x = 0;
    texture_window.y = (u8)entry->vram_y;
    texture_window.w = 0x100;
    texture_window.h = 0x100;
    for (i = 0; i < 2; i++) {
        g_option_at_list_pages[i].icons.icons[0] = &icons0[i];
        g_option_at_list_pages[i].icons.icons[1] = &icons1[i];
        g_option_at_list_pages[i].icons.icons[2] = &icons2[i];
        battle_menu_build_window_sprites((battle_menu_window_header_t*)&image_rect, (battle_menu_window_spec_t*)entry,
            (battle_menu_window_record_t*)&g_option_at_list_pages[i].icons);
    }
    battle_menu_set_disabled_texture_window((u8*)&page->draw_areas);
    SetDrawMode(
        &page->text_mode, 1, 0, GetTPage(0, 0, entry->vram_x & 0xFFC0, entry->vram_y & 0xFF00), &texture_window);
    for (i = 0, arrow_u = 0xD8; i < 2; i++) {
        battle_menu_init_semitransparent_sprt(&page->arrows[i]);
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
    battle_menu_init_semitransparent_sprt(&page->thumb);
    page->thumb.u0 = 0x10;
    page->thumb.v0 = 0x10;
    page->thumb.w = 8;
    page->thumb.h = 8;
    page->thumb.clut = 0x7D7C;
    battle_menu_init_semitransparent_sprt(&page->frame);
    battle_gfx_init_image_loading((POLY_FT4*)&page->frame, (const battle_image_location_t*)entry,
        (const battle_image_location_t*)&entry->window_x, (const world_gfx_image_load_parameters_t*)&window);
    battle_menu_init_sprite_array(page->arrow_marks, 2, 0x7C3C);
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
    buffer = g_option_menu_render_buffer;
    battle_clear_menu_render_buffer(buffer, entry->inner_width * entry->inner_height / 2);
    redraw_pending = 1;
    battle_thread_start(g_battle_current_thread_id + 1, option_menu_run_dead_unit_panel_a);
    battle_thread_set_parameters(g_battle_current_thread_id + 1, (s32)&entry->window_x, 0, 0);
    option_menu_render_entries((option_menu_entry_t*)entry, &row_offset, buffer);
    ring = 0;
    image_rect.w = g_menu_text_state.stride >> 2;
    LoadImage(&image_rect, (u32*)buffer);
    clear.h = 2;
    clear.x = image_rect.x;
    clear.y = ((image_rect.y - 2) & 0xFF) + (image_rect.y & 0xFF00);
    clear.w = image_rect.w;
    ClearImage(&clear, 0, 0, 0);
    battle_copy_bytes(&g_option_at_list_pages[1], g_option_at_list_pages, sizeof(world_menu_list_page_t));
    for (i = 0; i < 2; i++) {
        g_option_at_list_pages[i].icons.icons[0] = &icons0[i];
        g_option_at_list_pages[i].icons.icons[1] = &icons1[i];
        g_option_at_list_pages[i].icons.icons[2] = &icons2[i];
    }
    accum = 0;
    delta = 0;
    i = 0;
    for (;;) {
        page = &g_option_at_list_pages[i & 1];
        frame = &page->frame;
        if (battle_thread_is_previous_running() != 0 && g_event_mode == 0) {
            battle_thread_set_parameters(g_battle_current_thread_id + 1, (s32)&entry->window_x, 1, 0);
        } else {
            battle_thread_set_parameters(g_battle_current_thread_id + 1, (s32)&entry->window_x, 0, 0);
        }
        if (delta == 0 && battle_menu_can_accept_input() != 0 && i != 0) {
            /* Dead store (column is reinitialised before use); only the reload
             * of layout for this branch survives in the target. */
            if (layout == 0) {
                column = 0;
            }
            if ((*input & PSX_PAD_UP) && !(*input2 & PSX_PAD_SQUARE)) {
                if (cursor != 0) {
                    cursor--;
                    g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset != 0) {
                    delta = -1;
                    g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                } else if (layout->hidden_rows == 0) {
                    cursor = layout->row_count - 1;
                    if (layout->row_count != 1) {
                        g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                    }
                }
            } else if ((*input & PSX_PAD_DOWN) && !(*input2 & PSX_PAD_SQUARE)) {
                if (cursor != layout->row_count - 1) {
                    cursor++;
                    g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset != layout->hidden_rows) {
                    delta++;
                    g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset == 0) {
                    cursor = 0;
                    if (layout->row_count != 1) {
                        g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                    }
                }
            } else {
                timer = 0;
            }
            if (delta != 0) {
                do {
                    if (delta > 0) {
                        image_rect.y = (((ring + layout->row_count) * 16) & 0xFF) + entry->vram_y;
                    } else {
                        image_rect.y = (((ring - 1) * 16) & 0xFF) + entry->vram_y;
                    }
                    image_rect.h = 0x10;
                    buffer = g_option_menu_render_buffer;
                    battle_clear_menu_render_buffer(buffer, 0x800);
                    g_menu_text_state.stride = entry->inner_width;
                    for (column = 0; column < 3; column++) {
                        index = cursor + row_offset + delta;
                        battle_menu_set_text_origin(layout->x[column], 0);
                        if (layout->mode[column] != 2) {
                            g_menu_text_palette_offset = layout->text_colors[column][index];
                        }
                        if (g_menu_text_palette_offset == 4) {
                            g_menu_text_state.color = 0x44444444;
                        }
                        if (layout->mode[column] == 0) {
                            battle_menu_display_text_entry(
                                layout->text_ids[column][index], buffer, &g_menu_text_state.origin_x);
                        } else if (layout->mode[column] == 1) {
                            battle_display_menu_number_entry(
                                layout->text_ids[column][index], 2, (s32)buffer, (u16*)&g_menu_text_state.origin_x);
                        } else if (layout->mode[column] == 3) {
                            battle_draw_menu_number_glyphs(layout->text_ids[column][index], 2, buffer,
                                (battle_rect_t*)&g_menu_text_state.origin_x);
                        }
                    }
                    g_menu_text_state.color = 0;
                    g_menu_text_palette_offset = 0;
                    LoadImage(&image_rect, (u32*)buffer);
                    clear.h = 2;
                    clear.x = image_rect.x;
                    clear.y = ((image_rect.y - 2) & 0xFF) + (image_rect.y & 0xFF00);
                    clear.w = image_rect.w;
                    ClearImage(&clear, 0, 0, 0);
                } while (0);
            }
        }
        battle_thread_yield();
        if (redraw_pending != 0) {
            redraw_pending = 0;
        }
        if (delta == 0 && battle_menu_can_accept_input() != 0) {
            redraw_pending = (s32)((void* (*)(world_menu_entry_t*, s32*, s32*, s32*))option_menu_update_scroll)(
                entry, &row_offset, &ring, &cursor);
            ClearImage(&clear, 0, 0, 0);
        }
        entry->selected_index = cursor + row_offset;
        if (battle_menu_should_close_thread((s32*)input) != 0) {
            break;
        }
        if (delta == 0) {
            battle_menu_handle_action(entry, cursor);
        }
        battle_handle_menu_cancel_input(entry);
        if ((g_battle_event_speed == 1 && !(battle_script_get_event_initial_repeat_counter() & 1))
            || g_battle_event_speed == 2) {
            if (timer < g_main_menu_scroll_accel_delay) {
                step = g_main_menu_scroll_slow_step << 1;
            } else {
                step = g_main_menu_scroll_fast_step << 1;
            }
            accum += delta * step;
        }
        timer += g_battle_event_speed;
        if ((u32)(accum + 0xF) >= 0x1F) {
            ring += delta;
            row_offset += delta;
            repeat = battle_script_get_event_initial_repeat_counter();
            delta = 0;
            accum = 0;
            if (repeat >= g_main_input_repeat_initial_delay / g_battle_event_speed) {
                *input |= *input2 & (PSX_PAD_UP | PSX_PAD_DOWN);
            }
        }
        frame->v0 = ring * 16 + accum - 2;
        if (battle_thread_is_previous_running() != 0 || g_event_mode == 1) {
            frame->clut = 0x7F7D;
        } else {
            frame->clut = 0x7FFC;
        }
        if (layout->hidden_rows != 0 && battle_menu_can_accept_input() != 0) {
            if (row_offset != 0) {
                battle_gfx_draw_or_append_gpu_primitive((s32*)&page->arrows[0]);
                battle_gfx_draw_or_append_gpu_primitive((s32*)&page->arrow_marks[0]);
            }
            if (row_offset != layout->hidden_rows) {
                battle_gfx_draw_or_append_gpu_primitive((s32*)&page->arrows[1]);
                battle_gfx_draw_or_append_gpu_primitive((s32*)&page->arrow_marks[1]);
            }
            span = page->arrows[1].y0;
            top = page->arrows[0].y0 + page->arrows[0].h;
            span -= top;
            page->thumb.x0 = page->arrows[0].x0;
            page->thumb.y0 = top + span * entry->selected_index / (layout->row_count + layout->hidden_rows) - 2;
            battle_gfx_draw_or_append_gpu_primitive((s32*)&page->thumb);
        }
        battle_menu_build_zoom_draw_area_pair(&page->draw_areas, &entry->window_x, i, g_main_gfx_screen_polarity);
        battle_set_menu_sprite_clut_variant_1(page->icons.icons[0]);
        if ((s16)entry->header_id >= 5) {
            battle_gfx_draw_or_append_gpu_primitive((s32*)page->icons.icons[0]);
        }
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->draw_areas.areas[1]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->icon_mode);
        battle_gfx_draw_or_append_gpu_primitive((s32*)frame);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->text_mode);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->draw_areas);
        battle_menu_configure_frame_cluts((struct menu_frame_sprites*)&page->icons.base);
        battle_update_menu_cursor_primitives((world_menu_icon_thread_param_t*)entry, &page->icons.base, i, cursor);
        i++;
        page->icons.base.sprites[0].x0 = 1000;
        page->icons.base.sprites[1].y0 += 4;
        page->icons.base.sprites[2].y0 += 4;
        battle_menu_submit_frame_primitives((struct battle_menu_frame_primitives*)&page->icons.base);
    }
    battle_thread_set_parameters(g_battle_current_thread_id + 1, 0, 0, 1);
    battle_thread_yield();
    battle_thread_exit_current();
}
