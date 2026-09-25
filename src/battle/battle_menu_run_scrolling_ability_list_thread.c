#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Provisional: one of the two 0x134-byte packet pages the list alternates
 * between frames (BATTLE twin of the WORLD scroll-list page). */
typedef struct battle_menu_scroll_list_page {
    SPRT frame;                        /* 0x00: cursor frame */
    SPRT arrows[2];                    /* 0x14: scroll arrows */
    SPRT arrow_marks[2];               /* 0x3c */
    SPRT thumb;                        /* 0x64: scroll-bar thumb */
    DR_MODE text_mode;                 /* 0x78 */
    DR_MODE icon_mode;                 /* 0x84 */
    battle_menu_window_record_t icons; /* 0x90 */
    u8 _unused_10c[0x28];
} battle_menu_scroll_list_page_t;

struct battle_menu_text_position;
extern void battle_menu_draw_stacked_glyph_pair(void* pixels, struct battle_menu_text_position* position);

/*
 * Scrolling three-column ability list thread, the BATTLE twin of
 * world_menu_scrolling_list_thread (WORLD 0x800edaa8).
 *
 * Beyond the WORLD version, it nests through g_battle_menu_scroll_list_depth: nested lists keep
 * their packet pages at the event overlay + 0x1000 (level 2 also places the
 * text buffer at + 0x2000), and the outermost list saves the CLUT row at
 * g_battle_menu_ability_list_clut_rect into row 0 of a five-row buffer, derives four variants from it
 * and blinks rows 3/4 while it runs. Square/Circle on a row whose text is not
 * 0x7000 starts battle_menu_preview_ability_turn_at_list in the previous
 * thread slot (with entry 52 of g_battle_menu_thread_menu_data) and blinks rows 1/2 until
 * it finishes.
 *
 * Layout notes: the blink loop's exit block sits inside the loop as
 * `if (...) { ...; break; }` so loop.c moves it in front of the else-if test,
 * as in the target; `base` is copied in each branch so cross-jumping leaves
 * one spill store at the join; and the blink counter is cleared after the
 * thread setup so the scheduler keeps the entry address load ahead of the
 * thread-id load.
 */
void battle_menu_run_scrolling_ability_list_thread(void) {
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
    battle_menu_scroll_list_page_t* base;
    world_menu_scroll_text_layout_t* layout;
    s32 scroll_accum;
    s32 scroll_delta;
    s32 hold_time;
    s32 redraw_pending;
    s32 row_buffer_live;
    world_menu_entry_t* entry;
    battle_menu_scroll_list_page_t* page;
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
    u16* clut;
    u16* saved_clut;

    saved_clut = g_battle_menu_scratch_buffer.clut_save;
    row_buffer_live = 0;
    hold_time = 0;
    if (g_battle_menu_scroll_list_depth != 0) {
        /* Nested list: page pair in the event overlay area, 0x1000 bytes in. */
        page = (battle_menu_scroll_list_page_t*)(g_event_overlay_load_address + 0x1000);
        base = page;
    } else {
        /* Outermost list: page pair at +0x388 of the current subsystem workspace. */
        page = (battle_menu_scroll_list_page_t*)((u8*)g_battle_ai_workspace_ptr + 0x388);
        base = page;
    }
    toggle = 0;
    g_battle_menu_scroll_list_depth++;
    entry = (world_menu_entry_t*)battle_thread_get_current_parameter_1();
    layout = (world_menu_scroll_text_layout_t*)entry->text_binding;
    entry->window_height = layout->columns.row_count * 16 + 0x1C;
    entry->inner_height = layout->columns.row_count * 16 + 0x1C;
    entry->overall_height = layout->columns.row_count * 16 + 0x1C;
    window.rect.x = 0x108 - entry->inner_width;
    window.rect.y = 0xFE;
    window.x_screen_offset = 8;
    window.y_screen_offset = 0xC;
    window.rect.w = entry->inner_width - 8;
    window.rect.h = entry->window_height - 0x1A;
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
        base[i].icons.extra[0] = &icons0[i];
        base[i].icons.extra[1] = &icons1[i];
        base[i].icons.extra[2] = &icons2[i];
        battle_menu_build_window_sprites(
            (battle_menu_window_header_t*)&image_rect, (battle_menu_window_spec_t*)entry, &base[i].icons);
    }
    SetDrawMode(&page->icon_mode, 0, 0, GetTPage(0, 2, 0x3C0, 0x100), &g_battle_menu_disabled_texture_window);
    SetDrawMode(
        &page->text_mode, 1, 0, GetTPage(0, 1, entry->vram_x & 0xFFC0, entry->vram_y & 0xFF00), &texture_window);
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
    battle_menu_build_and_upload_window_frame_image(entry->inner_width, entry->inner_height, &image_rect, 2);
    if (g_battle_menu_scroll_list_depth == 2) {
        buffer = g_event_overlay_load_address + 0x2000;
    } else {
        StoreImage(&g_battle_menu_ability_list_clut_rect, (u32*)saved_clut);
        buffer = battle_menu_alloc_memory(entry->inner_width * entry->inner_height / 2);
    }
    battle_clear_menu_render_buffer(buffer, window.rect.w * window.rect.h / 2);
    row_offset = layout->row_offset;
    if (layout->columns.hidden_rows < row_offset) {
        row_offset = layout->columns.hidden_rows;
    }
    cursor = entry->selected_index - row_offset;
    if (cursor >= layout->columns.row_count) {
        row_offset = 0;
        cursor = 0;
    }
    battle_menu_draw_text_columns_narrow(entry, &row_offset, buffer);
    redraw_pending = 1;
    ring = 0;
    image_rect.x = window.rect.x / 4 + entry->vram_x;
    image_rect.y = 0x80;
    image_rect.w = window.rect.w / 4;
    image_rect.h = window.rect.h;
    LoadImage(&image_rect, (u32*)buffer);
    battle_copy_bytes(&base[1], base, sizeof(battle_menu_scroll_list_page_t));
    for (i = 0; i < 2; i++) {
        base[i].icons.extra[0] = &icons0[i];
        base[i].icons.extra[1] = &icons1[i];
        base[i].icons.extra[2] = &icons2[i];
    }
    i = 0;
    scroll_accum = 0;
    scroll_delta = 0;
    for (;;) {
        page = &base[i & 1];
        frame = &page->frame;
        SetSemiTrans(&frame, 1);
        if (scroll_delta == 0 && battle_menu_can_accept_input() != 0 && i != 0) {
            if (layout->columns.mode[0] == 0 && layout->columns.mode[1] == 0) {
                if (g_battle_script_event_input & (PSX_PAD_RIGHT | PSX_PAD_LEFT)) {
                    toggle ^= 1;
                }
                layout->columns.text_ids[2] = &toggle;
            } else if ((g_battle_script_event_input & (PSX_PAD_RIGHT | PSX_PAD_LEFT))
                && g_battle_menu_scroll_list_depth == 1 && layout->columns.mode[1] != 2 && i != 0
                && layout->columns.text_ids[0][entry->selected_index] != 0x7000) {
                battle_thread_yield();
                battle_sound_set_effect_to_cursor_move();
                battle_thread_start(g_battle_current_thread_id - 1, battle_menu_preview_ability_turn_at_list);
                battle_thread_set_parameters(
                    g_battle_current_thread_id - 1, (s32)&g_battle_menu_thread_menu_data[52], 0, 0);
                column = 0;
                while (1) {
                    if (battle_thread_is_running_8014cc94(g_battle_current_thread_id - 1) == 0) {
                        LoadImage(&g_battle_menu_ability_list_clut_rect, (u32*)(saved_clut + 48));
                        break;
                    }
                    if (column % 30 < 24) {
                        LoadImage(&g_battle_menu_ability_list_clut_rect, (u32*)(saved_clut + 16));
                    } else {
                        LoadImage(&g_battle_menu_ability_list_clut_rect, (u32*)(saved_clut + 32));
                    }
                    column++;
                    battle_thread_yield();
                }
            }
            if ((g_battle_script_event_input & PSX_PAD_UP)
                && (!(g_battle_script_event_current_input & PSX_PAD_SQUARE) || g_battle_menu_input_disabled != 0)) {
                if (cursor != 0) {
                    cursor--;
                    battle_sound_set_effect_to_cursor_move();
                } else if (row_offset != 0) {
                    scroll_delta = -1;
                    battle_sound_set_effect_to_cursor_move();
                } else if (layout->columns.hidden_rows == 0) {
                    cursor = layout->columns.row_count - 1;
                    battle_sound_set_effect_to_cursor_move();
                }
            } else if ((g_battle_script_event_input & PSX_PAD_DOWN)
                && (!(g_battle_script_event_current_input & PSX_PAD_SQUARE) || g_battle_menu_input_disabled != 0)) {
                if (cursor != layout->columns.row_count - 1) {
                    cursor++;
                    battle_sound_set_effect_to_cursor_move();
                } else if (row_offset != layout->columns.hidden_rows) {
                    scroll_delta++;
                    battle_sound_set_effect_to_cursor_move();
                } else if (row_offset == 0) {
                    cursor = 0;
                    battle_sound_set_effect_to_cursor_move();
                }
            } else {
                hold_time = 0;
            }
            if (scroll_delta != 0) {
                if (scroll_delta > 0) {
                    image_rect.y = (((ring + layout->columns.row_count) * 16) & 0x7F) + entry->vram_y;
                } else {
                    image_rect.y = (((ring - 1) * 16) & 0x7F) + entry->vram_y;
                }
                image_rect.h = 0x10;
                image_rect.y += 0x80;
                row_buffer_live = 1;
                buffer = battle_menu_alloc_memory(window.rect.w * 8);
                battle_clear_menu_render_buffer(buffer, window.rect.w * 8);
                g_menu_text_state.stride = window.rect.w;
                for (column = 0; column < 3; column++) {
                    index = cursor + row_offset + scroll_delta;
                    battle_menu_set_text_origin(layout->columns.x[column], 0);
                    if (layout->columns.mode[column] != 2) {
                        g_menu_text_palette_offset = layout->columns.text_colors[column][index];
                    }
                    g_menu_text_state.color = 0;
                    if (g_menu_text_palette_offset == 4) {
                        g_menu_text_state.color = 0x44444444;
                    }
                    if (layout->columns.mode[column] == 0) {
                        battle_menu_display_text_entry(
                            layout->columns.text_ids[column][index], buffer, &g_menu_text_state.origin_x);
                        g_menu_text_palette_offset = layout->columns.text_colors[column][index];
                        if (g_battle_thread_contexts[g_battle_current_thread_id].function_parameter_4 != 0
                            && (((world_ability_skill_use_tables_t*)g_battle_ai_workspace_ptr)->flags[index] & 1)) {
                            /* The target passes x and y as full words; the s16 prototype narrows them. */
                            ((void (*)(s32, s32))battle_menu_set_text_origin)(layout->columns.x[column] + 0x50, 0);
                            battle_menu_display_text_entry(0x5088, buffer, &g_menu_text_state.origin_x);
                        }
                    } else if (layout->columns.mode[column] == 1) {
                        battle_display_menu_number_entry(
                            layout->columns.text_ids[column][index], 2, (s32)buffer, (u16*)&g_menu_text_state.origin_x);
                    } else if (layout->columns.mode[column] == 3) {
                        if ((column == 2 || (column == 1 && layout->columns.mode[2] == 2))
                            && (u16)layout->columns.text_ids[column][index] < 2
                            && (((world_ability_skill_use_tables_t*)g_battle_ai_workspace_ptr)->target[index] == 0xFF
                                || ((world_ability_skill_use_tables_t*)g_battle_ai_workspace_ptr)->target[index]
                                    == 0)) {
                            g_menu_text_state.origin_y += 2;
                            battle_menu_draw_stacked_glyph_pair(
                                buffer, (struct battle_menu_text_position*)&g_menu_text_state.origin_x);
                        } else {
                            value = (u16)layout->columns.text_ids[column][index];
                            if (value & 0x7FF) {
                                if ((value & 0x7FF) >= 100) {
                                    g_menu_text_palette_offset = 0;
                                    g_menu_text_state.origin_x += 3;
                                    battle_menu_display_text_entry(0x509D, buffer, &g_menu_text_state.origin_x);
                                } else {
                                    battle_draw_menu_number_glyphs(
                                        (s16)value, 2, buffer, (battle_rect_t*)&g_menu_text_state.origin_x);
                                }
                            }
                        }
                    }
                }
                g_menu_text_palette_offset = 0;
                g_menu_text_state.color = 0;
                LoadImage(&image_rect, (u32*)buffer);
                clear.h = 2;
                clear.x = image_rect.x;
                clear.y = ((image_rect.y - 2) & 0xFF) + (image_rect.y & 0xFF00);
                clear.w = image_rect.w;
                ClearImage(&clear, 0, 0, 0);
            }
        }
        battle_thread_yield();
        if (i == 0) {
            for (column = 0; column < 16; column++) {
                clut = &saved_clut[column];
                clut[16] = clut[0];
                clut[32] = clut[0];
                clut[48] = clut[0];
                clut[64] = clut[0];
                if (column >= 4 && column < 8) {
                    clut[32] = 0x8000;
                    clut[16] = clut[-4];
                } else if (column >= 8) {
                    clut[64] = clut[56];
                }
            }
        }
        if (g_battle_menu_scroll_list_depth == 1) {
            if (i % 30 < 24) {
                LoadImage(&g_battle_menu_ability_list_clut_rect, (u32*)(saved_clut + 48));
            } else {
                LoadImage(&g_battle_menu_ability_list_clut_rect, (u32*)(saved_clut + 64));
            }
        }
        if (redraw_pending != 0) {
            battle_menu_free_memory(buffer);
            redraw_pending = 0;
        }
        if (scroll_delta == 0 && battle_menu_can_accept_input() != 0) {
            buffer
                = battle_menu_redraw_text_page_on_scroll_in_rect(entry, &row_offset, &ring, (s32)&cursor, &window.rect);
            redraw_pending = (s32)buffer;
            ClearImage(&clear, 0, 0, 0);
        }
        entry->selected_index = cursor + row_offset;
        layout->row_offset = row_offset;
        if (battle_menu_should_close_thread(&g_battle_script_event_input) != 0) {
            break;
        }
        battle_menu_configure_frame_cluts(&page->icons);
        battle_set_menu_sprite_clut_variant_1(page->icons.extra[0]);
        battle_set_menu_sprite_clut_variant_1(page->icons.extra[1]);
        battle_set_menu_sprite_clut_variant_1(page->icons.extra[2]);
        if (battle_thread_is_previous_running() != 0 || g_event_mode == 1) {
            frame->clut = 0x7F7D;
        } else {
            frame->clut = 0x7FFC;
        }
        battle_update_menu_cursor_primitives(
            (world_menu_icon_thread_param_t*)entry, (world_menu_icon_sprites_t*)&page->icons, i, cursor);
        if (layout->columns.hidden_rows != 0 && battle_thread_is_previous_running() == 0) {
            if (row_offset != 0) {
                battle_gfx_draw_or_append_gpu_primitive(&page->arrows[0]);
                battle_gfx_draw_or_append_gpu_primitive(&page->arrow_marks[0]);
            }
            if (row_offset != layout->columns.hidden_rows) {
                battle_gfx_draw_or_append_gpu_primitive(&page->arrows[1]);
                battle_gfx_draw_or_append_gpu_primitive(&page->arrow_marks[1]);
            }
            span = page->arrows[1].y0;
            top = page->arrows[0].y0 + page->arrows[0].h;
            span -= top;
            page->thumb.x0 = page->arrows[0].x0;
            page->thumb.y0
                = top + span * entry->selected_index / (layout->columns.row_count + layout->columns.hidden_rows) - 2;
            battle_gfx_draw_or_append_gpu_primitive(&page->thumb);
        }
        if (g_battle_menu_scroll_list_depth < 2 && scroll_delta == 0) {
            battle_menu_handle_action(entry, cursor);
        }
        battle_handle_menu_cancel_input(entry);
        if ((g_battle_event_speed == 1 && !(battle_script_get_event_initial_repeat_counter() & 1))
            || g_battle_event_speed == 2) {
            if (hold_time < g_main_menu_scroll_accel_delay) {
                step = g_main_menu_scroll_slow_step;
            } else {
                step = g_main_menu_scroll_fast_step;
            }
            scroll_accum += scroll_delta * (step << 1);
        }
        hold_time += g_battle_event_speed;
        if ((u32)(scroll_accum + 0xF) >= 0x1F) {
            ring += scroll_delta;
            row_offset += scroll_delta;
            repeat = battle_script_get_event_initial_repeat_counter();
            scroll_accum = 0;
            scroll_delta = 0;
            if (repeat >= g_main_input_repeat_initial_delay / g_battle_event_speed) {
                g_battle_script_event_input |= g_battle_script_event_current_input & (PSX_PAD_UP | PSX_PAD_DOWN);
            }
            battle_menu_free_memory(buffer);
            row_buffer_live = 0;
        }
        frame->v0 = (ring * 16 + scroll_accum - 2) & 0x7F;
        if (i * g_battle_event_speed < 12) {
            battle_gfx_init_image_loading((POLY_FT4*)page, (const battle_image_location_t*)entry,
                (const battle_image_location_t*)&entry->window_x, (const world_gfx_image_load_parameters_t*)&window);
            battle_menu_zoom_cursor_frame(entry, frame, i * g_battle_event_speed);
        }
        if (layout->columns.mode[1] == 0 && toggle == 1) {
            page->icons.sprites[1].x0 += ((world_menu_text_layout_t*)entry->text_binding)->x[1];
            page->icons.sprites[2].x0 += ((world_menu_text_layout_t*)entry->text_binding)->x[1];
        }
        page->icons.sprites[1].y0 += 4;
        page->icons.sprites[2].y0 += 4;
        if ((s16)entry->header_id >= 5) {
            battle_gfx_draw_or_append_gpu_primitive(page->icons.extra[0]);
            battle_gfx_draw_or_append_gpu_primitive(page->icons.extra[1]);
            if (g_battle_thread_contexts[g_battle_current_thread_id].function_parameter_4 != 0) {
                battle_gfx_draw_or_append_gpu_primitive(page->icons.extra[2]);
            }
        }
        battle_gfx_draw_or_append_gpu_primitive(&page->icons.sprites[3]);
        battle_gfx_draw_or_append_gpu_primitive(&page->icons.sprites[1]);
        battle_gfx_draw_or_append_gpu_primitive(&page->icons.sprites[2]);
        battle_gfx_draw_or_append_gpu_primitive(&page->icon_mode);
        battle_gfx_draw_or_append_gpu_primitive(frame);
        battle_gfx_draw_or_append_gpu_primitive(&page->text_mode);
        battle_gfx_draw_or_append_gpu_primitive(&page->icons.mode1);
        battle_gfx_draw_or_append_gpu_primitive(&page->icons.sprites[0]);
        battle_gfx_draw_or_append_gpu_primitive(&page->icons);
        i++;
    }
    if (g_battle_menu_scroll_list_depth != 2) {
        LoadImage(&g_battle_menu_ability_list_clut_rect, (u32*)saved_clut);
    }
    if (row_buffer_live != 0) {
        battle_menu_free_memory(buffer);
    }
    battle_thread_yield();
    g_battle_menu_scroll_list_depth--;
    if (battle_thread_get_current_parameter_2() == 0) {
        battle_thread_exit_current();
    }
}
