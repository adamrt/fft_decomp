#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_menu_window.h"
#include "fft/battle_text.h"
#include "fft/event.h"
#include "fft/main_gfx.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/menu_types.h"
#include "fft/option.h"
#include "fft/options.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Provisional: one of the two 0x190-byte options-menu packet pages at
 * 0x801cbc5c, alternated by frame (OPTION twin of the WORLD page at
 * 0x801ae894). */
typedef struct option_menu_page {
    SPRT title;                                   /* 0x000: window title image */
    SPRT value;                                   /* 0x014: selected value image */
    world_gfx_scaled_draw_area_pair_t title_area; /* 0x028 */
    world_gfx_scaled_draw_area_pair_t value_area; /* 0x050 */
    DR_MODE text_mode;                            /* 0x078 */
    DR_MODE window_mode;                          /* 0x084 */
    DR_MODE icon_mode;                            /* 0x090 */
    battle_menu_window_record_t icons;            /* 0x09c */
    SPRT frame;                                   /* 0x118: cursor frame */
    SPRT arrows[2];                               /* 0x12c: scroll arrows */
    SPRT arrow_marks[2];                          /* 0x154 */
    SPRT thumb;                                   /* 0x17c: scroll-bar thumb */
} option_menu_page_t;

typedef char option_menu_page_size_must_be_0x190[(sizeof(option_menu_page_t) == 0x190) ? 1 : -1];

/* Text image placement: the upload rectangle plus the screen offsets
 * battle_gfx_init_image_loading reads. */
typedef struct option_menu_text_window {
    RECT rect;
    s16 x_screen_offset;
    s16 y_screen_offset;
} option_menu_text_window_t;

struct menu_frame_sprites;
void battle_gfx_init_image_loading(POLY_FT4* primitive, const battle_image_location_t* base_load,
    const battle_image_location_t* base_screen, const world_gfx_image_load_parameters_t* parameters);

extern world_input_timing_profile_t g_option_input_repeat_timings[];
extern option_menu_page_t g_option_menu_pages[2];

/*
 * Options-menu thread, the OPTION twin of world_build_options_menu (same code,
 * globals and callees rebound) without its g_world_menu_thread_result stores.
 *
 * The do/while (0) around the thread lookup is a scheduling barrier: its
 * loop notes keep the input-1 spill and the entry load ahead of the
 * row-count loads, as in the target.
 */
void option_build_options_menu(void) {
    option_menu_text_window_t window;
    RECT image_rect;
    RECT clear;
    RECT texture_window;
    RECT value_rect;
    s32 row_offset;
    s32 ring;
    s32 cursor;
    battle_thread_t* thread;
    u8* buffer;
    u32* input0;
    u32* input1;
    s32 accum;
    s32 timer;
    world_menu_entry_t* entry;
    option_menu_page_t* page;
    option_menu_page_t* base;
    option_menu_text_layout_t* layout;
    s32 delta;
    s32 i;
    s32 column;
    s32 index;
    s32 step;
    s32 top;
    s32 span;
    s32 arrow_u;
    s32 arrow_y;
    s32 repeat;
    SPRT* arrow;
    SPRT* frame;
    u8* image;

    row_offset = 0;
    ring = 0;
    cursor = 0;
    input0 = battle_script_get_controller_input_pointer(0);
    input1 = battle_script_get_controller_input_pointer(1);
    do {
        thread = &g_battle_threads[g_battle_current_thread_id];
        entry = (world_menu_entry_t*)thread->function_parameter_1;
    } while (0);
    page = g_option_menu_pages;
    layout = g_option_menu_entry_table;
    entry->window_height = layout->row_count * 16 + 0x10;
    entry->inner_height = layout->row_count * 16 + 0x10;
    entry->overall_height = layout->row_count * 16 + 0x10;
    window.x_screen_offset = 8;
    window.rect.x = 0;
    window.rect.y = 0;
    window.y_screen_offset = 0xA;
    window.rect.w = entry->window_width - 0x20;
    window.rect.h = entry->window_height - 0xE;
    battle_menu_build_window_sprites(
        (battle_menu_window_header_t*)&image_rect, (battle_menu_window_spec_t*)entry, &page->icons);
    buffer = battle_menu_alloc_memory(0x1E0);
    g_option_menu_pages[0].icons.sprites[3].y0 -= 2;
    clear.x = entry->vram_x;
    clear.y = 0xFE;
    clear.w = window.rect.w / 4;
    clear.h = 2;
    ClearImage(&clear, 0, 0, 0);
    battle_gfx_set_draw_mode_for_texture_page(&page->icon_mode, 0);
    battle_gfx_set_draw_mode_for_texture_page(&page->text_mode, 1);
    texture_window.x = 0;
    texture_window.y = 0;
    texture_window.w = 0x100;
    texture_window.h = 0x100;
    SetDrawMode(
        &page->window_mode, 0, 0, GetTPage(0, 0, entry->vram_x & 0xFFC0, entry->vram_y & 0xFF00), &texture_window);
    arrow = page->arrows;
    for (i = 0, arrow_u = 0xD8; i < 2; i++) {
        battle_menu_init_semitransparent_sprt(&arrow[i]);
        page->arrows[i].clut = 0x7D7C;
        arrow[i].w = 8;
        arrow[i].h = 0x10;
        arrow[i].u0 = arrow_u;
        arrow[i].v0 = 0;
        arrow[i].x0 = entry->window_x + entry->window_width - 0xF;
        arrow_y = i * (entry->window_height - 0x20) + 8;
        arrow[i].y0 = entry->window_y + arrow_y;
        arrow_u += 8;
    }
    base = g_option_menu_pages;
    base->arrows[0].y0 += 4;
    battle_menu_init_semitransparent_sprt(&base->thumb);
    base->thumb.u0 = 0x10;
    base->thumb.v0 = 0x10;
    base->thumb.w = 8;
    base->thumb.h = 8;
    base->thumb.clut = 0x7D7C;
    battle_menu_init_semitransparent_sprt(&base->frame);
    battle_gfx_init_image_loading((POLY_FT4*)&base->frame, (const battle_image_location_t*)entry,
        (const battle_image_location_t*)&entry->window_x, (const world_gfx_image_load_parameters_t*)&window);
    battle_menu_init_sprite_array(base->arrow_marks, 2, 0x7C3C);
    base->arrow_marks[0].w = 6;
    base->arrow_marks[0].h = 0xC;
    base->arrow_marks[1].w = 4;
    base->arrow_marks[1].h = 4;
    base->arrow_marks[0].u0 = 0xE8;
    base->arrow_marks[1].u0 = 0xE8;
    base->arrow_marks[0].v0 = 0;
    base->arrow_marks[0].x0 = base->arrows[0].x0 - 1;
    base->arrow_marks[0].y0 = base->arrows[0].y0 + 3;
    base->arrow_marks[1].v0 = 0xC;
    base->arrow_marks[1].x0 = base->arrows[1].x0;
    base->arrow_marks[1].y0 = base->arrows[1].y0 + 0xE;
    image = g_option_menu_render_buffer;
    delta = 0;
    i = 0;
    accum = 0;
    timer = 0;
    battle_clear_menu_render_buffer(image, entry->inner_width * entry->inner_height / 2);
    option_menu_render_entries((option_menu_entry_t*)entry, &row_offset, image);
    ring = 0;
    image_rect.w = g_menu_text_state.stride >> 2;
    LoadImage(&image_rect, (u32*)image);
    battle_thread_wait_frames(1);
    battle_menu_set_disabled_texture_window((u8*)&page->title_area);
    battle_menu_set_disabled_texture_window((u8*)&page->value_area);
    battle_menu_init_semitransparent_sprt(&base->title);
    battle_text_configure_sprite_vram(&g_option_menu_title_image_rect, entry->inner_width, 0x2C, &base->title, 0);
    image
        = battle_menu_build_and_upload_window_frame_image(entry->inner_width, 0x2C, &g_option_menu_title_image_rect, 3);
    base->title.x0 = entry->window_x;
    base->title.y0 = entry->window_y + entry->window_height + 1;
    g_menu_text_state.stride = entry->inner_width;
    battle_menu_set_text_origin(0x18, 0xE);
    battle_menu_display_text_entry(0x502B, image, &g_menu_text_state.origin_x);
    LoadImage(&g_option_menu_title_image_rect, (u32*)image);
    battle_thread_wait_frames(1);
    battle_menu_free_memory(image);
    value_rect.x = base->title.x0;
    value_rect.y = base->title.y0;
    value_rect.w = base->title.w;
    value_rect.h = base->title.h;
    battle_copy_bytes(&g_option_menu_entry_panel_rect, &entry->window_x, 8);
    battle_thread_start(g_battle_current_thread_id + 1, option_menu_run_dead_unit_panel_a);
    g_option_menu_entry_panel_rect.h += 0xC;
    g_option_menu_entry_panel_rect.y -= 2;
    battle_thread_set_parameters(g_battle_current_thread_id + 1, (s32)&g_option_menu_entry_panel_rect, 0, 0);
    battle_thread_yield();
    battle_menu_init_semitransparent_sprt(&base->value);
    battle_text_configure_sprite_vram(&g_option_menu_value_image_rect, 0x3C, 0x10, &base->value, 0);
    base->value.x0 = entry->window_x + 0x5C;
    base->value.y0 = entry->window_y + entry->window_height + 0xF;
    battle_clear_menu_render_buffer(buffer, 0x1E0);
    g_menu_text_state.stride = 0x3C;
    battle_menu_set_text_origin(0, 0);
    battle_menu_display_text_entry(0x5021, buffer, &g_menu_text_state.origin_x);
    LoadImage(&g_option_menu_value_image_rect, (u32*)buffer);
    battle_copy_bytes(&base[1], base, sizeof(option_menu_page_t));
    for (;;) {
        page = &g_option_menu_pages[i & 1];
        if (delta == 0 && battle_menu_can_accept_input() != 0) {
            if ((*input0 & PSX_PAD_UP) && (!(*input1 & PSX_PAD_SQUARE) || g_battle_menu_input_disabled != 0)) {
                if (cursor != 0) {
                    cursor--;
                    g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset != 0) {
                    delta = -1;
                    g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                } else if (layout->hidden_rows == 0) {
                    cursor = layout->row_count - 1;
                    g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                }
            } else if ((*input0 & PSX_PAD_DOWN) && (!(*input1 & PSX_PAD_SQUARE) || g_battle_menu_input_disabled != 0)) {
                if (cursor != layout->row_count - 1) {
                    cursor++;
                    g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset != layout->hidden_rows) {
                    delta++;
                    g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset == 0) {
                    cursor = 0;
                    g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                }
            } else {
                timer = 0;
            }
            if (delta != 0) {
                if (delta > 0) {
                    image_rect.y = ((ring + layout->row_count) * 16) & 0xFF;
                } else {
                    image_rect.y = ((ring - 1) * 16) & 0xFF;
                }
                image = g_option_menu_render_buffer;
                image_rect.h = 0x10;
                battle_clear_menu_render_buffer(image, entry->inner_width * 8);
                g_menu_text_state.stride = entry->inner_width;
                for (column = 0; column < 2; column++) {
                    if (layout->text_ids[column] != (s16*)-1) {
                        index = cursor + row_offset + delta;
                        battle_menu_set_text_origin(layout->x[column], 0);
                        g_menu_text_palette_offset = layout->text_colors[column][index];
                        if (layout->mode[column] == 0) {
                            battle_menu_display_text_entry(
                                layout->text_ids[column][index], image, &g_menu_text_state.origin_x);
                        } else {
                            battle_display_menu_number_entry(
                                layout->text_ids[column][index], 2, (s32)image, (u16*)&g_menu_text_state.origin_x);
                        }
                    }
                }
                LoadImage(&image_rect, (u32*)image);
            }
            if (*input0 & PSX_PAD_CIRCLE) {
                g_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
                if (battle_menu_can_accept_input() != 0) {
                    *input0 = 0;
                    index = cursor + row_offset + delta;
                    battle_thread_start(g_battle_current_thread_id - 1, option_run_option_choice_menu);
                    battle_thread_set_parameters(g_battle_current_thread_id - 1, index, 0, 0);
                }
            }
            if (*input0 & PSX_PAD_CROSS) {
                battle_handle_menu_cancel_input(entry);
            }
        }
        if (g_event_mode == 0) {
            index = cursor + row_offset + delta;
            battle_clear_menu_render_buffer(buffer, 0x1E0);
            g_menu_text_state.stride = 0x3C;
            battle_menu_set_text_origin(0, 0);
            battle_menu_display_text_entry(g_option_value_text_id_bases[index] + g_option_current_values[index], buffer,
                &g_menu_text_state.origin_x);
            LoadImage(&g_option_menu_value_image_rect, (u32*)buffer);
        }
        battle_thread_yield();
        if (battle_menu_should_close_thread((s32*)input0) != 0) {
            break;
        }
        if (delta == 0 && battle_menu_can_accept_input() != 0) {
            ((void* (*)(world_menu_entry_t*, s32*, s32*, s32*))option_menu_update_scroll)(
                entry, &row_offset, &ring, &cursor);
            ClearImage(&clear, 0, 0, 0);
        }
        battle_menu_configure_frame_cluts((struct menu_frame_sprites*)&page->icons);
        battle_update_menu_cursor_primitives(
            (world_menu_icon_thread_param_t*)entry, (world_menu_icon_sprites_t*)&page->icons, i, cursor);
        page->icons.sprites[1].y0 += 2;
        page->icons.sprites[2].y0 += 2;
        entry->selected_index = row_offset + cursor;
        if ((g_battle_event_speed == 1 && !(battle_script_get_event_initial_repeat_counter() & 1))
            || g_battle_event_speed == 2) {
            if (timer < g_main_menu_scroll_accel_delay) {
                step = g_main_menu_scroll_slow_step * 2;
            } else {
                step = g_main_menu_scroll_fast_step * 2;
            }
            accum += delta * step;
        }
        timer += g_battle_event_speed;
        frame = &page->frame;
        if ((u32)(accum + 0xF) >= 0x1F) {
            ring += delta;
            row_offset += delta;
            repeat = battle_script_get_event_initial_repeat_counter();
            delta = 0;
            accum = 0;
            if (repeat >= g_main_input_repeat_initial_delay / g_battle_event_speed) {
                *input0 |= *input1 & (PSX_PAD_UP | PSX_PAD_DOWN);
            }
        }
        page->frame.v0 = ring * 16 + accum - 2;
        battle_menu_select_sprite_clut(&page->title);
        battle_menu_select_sprite_clut(&page->value);
        battle_menu_select_sprite_clut(frame);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->icons.sprites[3]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->icons.sprites[1]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->icons.sprites[2]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->icons.mode1);
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
        if ((thread - 1)->is_running == 0) {
            battle_thread_set_parameters(g_battle_current_thread_id + 1, (s32)&entry->window_x, 0, 0);
        } else {
            battle_thread_set_parameters(g_battle_current_thread_id + 1, (s32)&entry->window_x, 1, 0);
        }
        battle_menu_build_zoom_draw_area_pair(&page->title_area, &entry->window_x, i, g_main_gfx_screen_polarity);
        battle_menu_build_zoom_draw_area_pair(&page->value_area, &value_rect, i, g_main_gfx_screen_polarity);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->title_area.areas[1]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->icon_mode);
        battle_gfx_draw_or_append_gpu_primitive((s32*)frame);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->window_mode);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->title_area.areas[0]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->value_area.areas[1]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->value);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->title);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->text_mode);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->value_area.areas[0]);
        g_battle_thread_call_target = (void (*)(void))main_sound_set_type;
        g_main_game_options.fields.cursor_movement = g_option_current_values[GAME_OPTION_CURSOR_MOVEMENT];
        g_main_game_options.fields.cursor_repeat_speed = g_option_current_values[GAME_OPTION_CURSOR_REPEAT_SPEED];
        g_main_game_options.fields.multi_height_cursor_speed
            = g_option_current_values[GAME_OPTION_MULTI_HEIGHT_CURSOR_SPEED];
        g_main_game_options.fields.finger_cursor_repeat_speed
            = g_option_current_values[GAME_OPTION_FINGER_CURSOR_REPEAT_SPEED];
        g_main_game_options.fields.message_display_speed = g_option_current_values[GAME_OPTION_MESSAGE_DISPLAY_SPEED];
        g_main_game_options.fields.navigation_messages = g_option_current_values[GAME_OPTION_NAVIGATION_MESSAGES];
        g_main_game_options.fields.ability_names = g_option_current_values[GAME_OPTION_ABILITY_NAMES];
        g_main_game_options.fields.effect_messages = g_option_current_values[GAME_OPTION_EFFECT_MESSAGES];
        g_main_game_options.fields.display_gained_exp_jp = g_option_current_values[GAME_OPTION_DISPLAY_GAINED_EXP_JP];
        g_main_game_options.fields.target_flashing = g_option_current_values[GAME_OPTION_TARGET_FLASHING];
        g_main_game_options.fields.show_unequippable_items
            = g_option_current_values[GAME_OPTION_SHOW_UNEQUIPPABLE_ITEMS];
        g_main_game_options.fields.max_equip_at_job_change
            = g_option_current_values[GAME_OPTION_MAX_EQUIP_AT_JOB_CHANGE];
        g_main_game_options.fields.sound_mode = g_option_current_values[GAME_OPTION_SOUND_MODE];
        battle_thread_call_on_main_stack(g_option_current_values[GAME_OPTION_SOUND_MODE]);
        i++;
        column = g_main_game_options.fields.finger_cursor_repeat_speed;
        g_main_input_repeat_initial_delay = g_option_input_repeat_timings[column].initial_delay;
        g_main_input_repeat_period = g_option_input_repeat_timings[column].repeat_period;
        g_main_input_secondary_repeat_period = g_option_input_repeat_timings[column].secondary_repeat_period;
        g_main_menu_scroll_accel_delay = g_option_input_repeat_timings[column].scroll_accel_delay;
        g_main_menu_scroll_slow_step = g_option_input_repeat_timings[column].scroll_slow_step;
        g_main_menu_scroll_fast_step = g_option_input_repeat_timings[column].scroll_fast_step;
    }
    battle_menu_free_memory(buffer);
    battle_gfx_free_tpage7_vram(&g_option_menu_title_image_rect);
    battle_gfx_free_tpage7_vram(&g_option_menu_value_image_rect);
    battle_thread_wait_frames(1);
    battle_thread_set_parameters(g_battle_current_thread_id + 1, 0, 0, 1);
    battle_thread_exit_current();
}
