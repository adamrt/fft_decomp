#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/options.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Provisional: one of the two 0x190-byte options-menu packet pages at
 * 0x801ae894, alternated by frame. */
typedef struct world_options_menu_page {
    SPRT title;                                   /* 0x000: window title image */
    SPRT value;                                   /* 0x014: selected value image */
    world_gfx_scaled_draw_area_pair_t title_area; /* 0x028 */
    world_gfx_scaled_draw_area_pair_t value_area; /* 0x050 */
    DR_MODE text_mode;                            /* 0x078 */
    DR_MODE window_mode;                          /* 0x084 */
    DR_MODE icon_mode;                            /* 0x090 */
    world_menu_icon_record_t icons;               /* 0x09c */
    SPRT frame;                                   /* 0x118: cursor frame */
    SPRT arrows[2];                               /* 0x12c: scroll arrows */
    SPRT arrow_marks[2];                          /* 0x154 */
    SPRT thumb;                                   /* 0x17c: scroll-bar thumb */
} world_options_menu_page_t;

extern world_options_menu_page_t g_world_options_menu_pages[2];

/*
 * Options-menu thread started by world_menu_run_main_mode (mode 0) with
 * entry->text_binding = &g_world_option_menu_entry_table.
 *
 * Scrolls the two-column option list like world_menu_scrolling_list_thread, shows the
 * selected option's value text (g_world_option_menu_first_text_ids[i] + g_world_game_option_values[i]),
 * starts world_run_option_choice_menu on confirm, and each frame packs the option
 * values back into g_main_game_options, applies the sound mode on the main
 * stack and reloads the input-repeat timings for the finger-cursor speed.
 *
 * The do/while (0) around the thread lookup is a scheduling barrier: its
 * loop notes keep the input-1 spill and the entry load ahead of the
 * row-count loads, as in the target.
 */
void world_build_options_menu(void) {
    world_menu_text_window_t window;
    RECT image_rect;
    RECT clear;
    RECT texture_window;
    RECT value_rect;
    s32 row_offset;
    s32 ring;
    s32 cursor;
    native_thread_t* thread;
    u8* buffer;
    u32* input0;
    u32* input1;
    s32 accum;
    s32 timer;
    world_menu_entry_t* entry;
    world_options_menu_page_t* page;
    world_options_menu_page_t* base;
    world_menu_text_layout_t* layout;
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
    g_world_menu_thread_result = 1;
    input0 = world_input_get_menu_controller(0);
    input1 = world_input_get_menu_controller(1);
    do {
        thread = &g_world_threads[g_world_thread_current_id];
        entry = (world_menu_entry_t*)thread->function_parameter_1;
    } while (0);
    page = g_world_options_menu_pages;
    layout = (world_menu_text_layout_t*)&g_world_option_menu_entry_table;
    entry->window_height = layout->row_count * 16 + 0x10;
    entry->inner_height = layout->row_count * 16 + 0x10;
    entry->overall_height = layout->row_count * 16 + 0x10;
    window.x_screen_offset = 8;
    window.rect.x = 0;
    window.rect.y = 0;
    window.y_screen_offset = 0xA;
    window.rect.w = entry->window_width - 0x20;
    window.rect.h = entry->window_height - 0xE;
    world_menu_build_icon_record(&image_rect, (world_menu_icon_thread_param_t*)entry, &page->icons);
    buffer = world_menu_alloc_ui_buffer(0x1E0);
    g_world_options_menu_pages[0].icons.base.sprites[3].y0 -= 2;
    clear.x = entry->vram_x;
    clear.y = 0xFE;
    clear.w = window.rect.w / 4;
    clear.h = 2;
    ClearImage(&clear, 0, 0, 0);
    world_gfx_set_image_draw_mode(&page->icon_mode, 0);
    world_gfx_set_image_draw_mode(&page->text_mode, 1);
    texture_window.x = 0;
    texture_window.y = 0;
    texture_window.w = 0x100;
    texture_window.h = 0x100;
    SetDrawMode(
        &page->window_mode, 0, 0, GetTPage(0, 0, entry->vram_x & 0xFFC0, entry->vram_y & 0xFF00), &texture_window);
    arrow = page->arrows;
    for (i = 0, arrow_u = 0xD8; i < 2; i++) {
        world_menu_init_sprite(&arrow[i]);
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
    base = g_world_options_menu_pages;
    base->arrows[0].y0 += 4;
    world_menu_init_sprite(&base->thumb);
    base->thumb.u0 = 0x10;
    base->thumb.v0 = 0x10;
    base->thumb.w = 8;
    base->thumb.h = 8;
    base->thumb.clut = 0x7D7C;
    world_menu_init_sprite(&base->frame);
    world_gfx_init_image_loading((POLY_FT4*)&base->frame, (const world_image_location_t*)entry,
        (const world_image_location_t*)&entry->window_x, (const world_gfx_image_load_parameters_t*)&window);
    world_menu_init_sprite_array(base->arrow_marks, 2, 0x7C3C);
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
    image = g_world_menu_text_page_image;
    delta = 0;
    i = 0;
    accum = 0;
    timer = 0;
    world_clear_menu_render_buffer(image, entry->inner_width * entry->inner_height / 2);
    world_menu_draw_text_columns_2(entry, &row_offset, image);
    ring = 0;
    image_rect.w = g_world_menu_text_state.stride >> 2;
    LoadImage(&image_rect, (u32*)image);
    world_thread_wait_frames(1);
    world_gfx_reset_record_texture_window(&page->title_area);
    world_gfx_reset_record_texture_window(&page->value_area);
    world_menu_init_sprite(&base->title);
    world_menu_init_icon_slot(
        &g_world_options_menu_title_rect, entry->inner_width, 0x2C, (world_texture_prim_t*)&base->title, 0);
    image
        = world_menu_build_and_upload_window_frame_image(entry->inner_width, 0x2C, &g_world_options_menu_title_rect, 3);
    base->title.x0 = entry->window_x;
    base->title.y0 = entry->window_y + entry->window_height + 1;
    g_world_menu_text_state.stride = entry->inner_width;
    world_menu_set_text_origin(0x18, 0xE);
    world_menu_display_text_entry(0x502B, image, &g_world_menu_text_state.origin_x);
    LoadImage(&g_world_options_menu_title_rect, (u32*)image);
    world_thread_wait_frames(1);
    world_menu_free_memory(image);
    value_rect.x = base->title.x0;
    value_rect.y = base->title.y0;
    value_rect.w = base->title.w;
    value_rect.h = base->title.h;
    world_script_copy_bytes(&g_world_options_menu_window_rect, &entry->window_x, 8);
    world_thread_start(g_world_thread_current_id + 1, world_menu_window_frame_thread);
    g_world_options_menu_window_rect.h += 0xC;
    g_world_options_menu_window_rect.y -= 2;
    world_thread_set_parameters(g_world_thread_current_id + 1, (s32)&g_world_options_menu_window_rect, 0, 0);
    world_thread_yield();
    world_menu_init_sprite(&base->value);
    world_menu_init_icon_slot(&g_world_options_menu_value_rect, 0x3C, 0x10, (world_texture_prim_t*)&base->value, 0);
    base->value.x0 = entry->window_x + 0x5C;
    base->value.y0 = entry->window_y + entry->window_height + 0xF;
    world_clear_menu_render_buffer(buffer, 0x1E0);
    g_world_menu_text_state.stride = 0x3C;
    world_menu_set_text_origin(0, 0);
    world_menu_display_text_entry(0x5021, buffer, &g_world_menu_text_state.origin_x);
    LoadImage(&g_world_options_menu_value_rect, (u32*)buffer);
    world_script_copy_bytes(&base[1], base, sizeof(world_options_menu_page_t));
    for (;;) {
        page = &g_world_options_menu_pages[i & 1];
        if (delta == 0 && world_menu_is_input_allowed() != 0) {
            if ((*input0 & PSX_PAD_UP) && (!(*input1 & PSX_PAD_SQUARE) || g_world_menu_input_disabled != 0)) {
                if (cursor != 0) {
                    cursor--;
                    g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset != 0) {
                    delta = -1;
                    g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                } else if (layout->hidden_rows == 0) {
                    cursor = layout->row_count - 1;
                    g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                }
            } else if ((*input0 & PSX_PAD_DOWN) && (!(*input1 & PSX_PAD_SQUARE) || g_world_menu_input_disabled != 0)) {
                if (cursor != layout->row_count - 1) {
                    cursor++;
                    g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset != layout->hidden_rows) {
                    delta++;
                    g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset == 0) {
                    cursor = 0;
                    g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
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
                image = g_world_menu_text_page_image;
                image_rect.h = 0x10;
                world_clear_menu_render_buffer(image, entry->inner_width * 8);
                g_world_menu_text_state.stride = entry->inner_width;
                for (column = 0; column < 2; column++) {
                    if (layout->text_ids[column] != (s16*)-1) {
                        index = cursor + row_offset + delta;
                        world_menu_set_text_origin(layout->x[column], 0);
                        g_world_menu_text_color = layout->text_colors[column][index];
                        if (layout->mode[column] == 0) {
                            world_menu_display_text_entry(
                                layout->text_ids[column][index], image, &g_world_menu_text_state.origin_x);
                        } else {
                            world_display_menu_number_entry(layout->text_ids[column][index], 2, (s32)image,
                                (u16*)&g_world_menu_text_state.origin_x);
                        }
                    }
                }
                LoadImage(&image_rect, (u32*)image);
            }
            if (*input0 & PSX_PAD_CIRCLE) {
                g_world_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
                if (world_menu_is_input_allowed() != 0) {
                    *input0 = 0;
                    index = cursor + row_offset + delta;
                    world_thread_start(g_world_thread_current_id - 1, world_run_option_choice_menu);
                    world_thread_set_parameters(g_world_thread_current_id - 1, index, 0, 0);
                }
            }
            if (*input0 & PSX_PAD_CROSS) {
                world_menu_cancel_thread_group((world_menu_cancel_context_t*)entry);
            }
        }
        if (g_world_thread_task_active == 0) {
            index = cursor + row_offset + delta;
            world_clear_menu_render_buffer(buffer, 0x1E0);
            g_world_menu_text_state.stride = 0x3C;
            world_menu_set_text_origin(0, 0);
            world_menu_display_text_entry(g_world_option_menu_first_text_ids[index] + g_world_game_option_values[index],
                buffer, &g_world_menu_text_state.origin_x);
            LoadImage(&g_world_options_menu_value_rect, (u32*)buffer);
        }
        world_thread_yield();
        if (world_menu_check_thread_completion((s32*)input0) != 0) {
            break;
        }
        if (delta == 0 && world_menu_is_input_allowed() != 0) {
            ((u8 * (*)(world_menu_entry_t*, s32*, s32*, s32*)) world_menu_redraw_text_page_on_scroll)(
                entry, &row_offset, &ring, &cursor);
            ClearImage(&clear, 0, 0, 0);
        }
        world_menu_select_icon_cluts(&page->icons.base);
        world_menu_update_icon_cursor_sprites((world_menu_icon_thread_param_t*)entry, &page->icons.base, i, cursor);
        page->icons.base.sprites[1].y0 += 2;
        page->icons.base.sprites[2].y0 += 2;
        entry->selected_index = row_offset + cursor;
        if ((g_world_event_speed == 1 && !(world_input_get_menu_repeat_counter() & 1)) || g_world_event_speed == 2) {
            if (timer < g_main_menu_scroll_accel_delay) {
                step = g_main_menu_scroll_slow_step * 2;
            } else {
                step = g_main_menu_scroll_fast_step * 2;
            }
            accum += delta * step;
        }
        timer += g_world_event_speed;
        frame = &page->frame;
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
        page->frame.v0 = ring * 16 + accum - 2;
        world_menu_select_task_icon_tile((world_texture_prim_t*)&page->title);
        world_menu_select_task_icon_tile((world_texture_prim_t*)&page->value);
        world_menu_select_task_icon_tile((world_texture_prim_t*)frame);
        world_gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[3]);
        world_gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[1]);
        world_gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[2]);
        world_gfx_draw_or_append_gpu_primitive(&page->icons.base.draw_mode);
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
        if ((thread - 1)->is_running == 0) {
            world_thread_set_parameters(g_world_thread_current_id + 1, (s32)&entry->window_x, 0, 0);
        } else {
            world_thread_set_parameters(g_world_thread_current_id + 1, (s32)&entry->window_x, 1, 0);
        }
        world_gfx_build_scaled_draw_area_pair_swapped(
            &page->title_area, &entry->window_x, i, g_active_graphics_buffer_index);
        world_gfx_build_scaled_draw_area_pair_swapped(
            &page->value_area, &value_rect, i, g_active_graphics_buffer_index);
        world_gfx_draw_or_append_gpu_primitive(&page->title_area.areas[1]);
        world_gfx_draw_or_append_gpu_primitive(&page->icon_mode);
        world_gfx_draw_or_append_gpu_primitive(frame);
        world_gfx_draw_or_append_gpu_primitive(&page->window_mode);
        world_gfx_draw_or_append_gpu_primitive(&page->title_area.areas[0]);
        world_gfx_draw_or_append_gpu_primitive(&page->value_area.areas[1]);
        world_gfx_draw_or_append_gpu_primitive(&page->value);
        world_gfx_draw_or_append_gpu_primitive(&page->title);
        world_gfx_draw_or_append_gpu_primitive(&page->text_mode);
        world_gfx_draw_or_append_gpu_primitive(&page->value_area.areas[0]);
        g_world_thread_call_target = (void (*)(void))main_sound_set_type;
        g_main_game_options.fields.cursor_movement = g_world_game_option_values[GAME_OPTION_CURSOR_MOVEMENT];
        g_main_game_options.fields.cursor_repeat_speed = g_world_game_option_values[GAME_OPTION_CURSOR_REPEAT_SPEED];
        g_main_game_options.fields.multi_height_cursor_speed
            = g_world_game_option_values[GAME_OPTION_MULTI_HEIGHT_CURSOR_SPEED];
        g_main_game_options.fields.finger_cursor_repeat_speed
            = g_world_game_option_values[GAME_OPTION_FINGER_CURSOR_REPEAT_SPEED];
        g_main_game_options.fields.message_display_speed
            = g_world_game_option_values[GAME_OPTION_MESSAGE_DISPLAY_SPEED];
        g_main_game_options.fields.navigation_messages = g_world_game_option_values[GAME_OPTION_NAVIGATION_MESSAGES];
        g_main_game_options.fields.ability_names = g_world_game_option_values[GAME_OPTION_ABILITY_NAMES];
        g_main_game_options.fields.effect_messages = g_world_game_option_values[GAME_OPTION_EFFECT_MESSAGES];
        g_main_game_options.fields.display_gained_exp_jp
            = g_world_game_option_values[GAME_OPTION_DISPLAY_GAINED_EXP_JP];
        g_main_game_options.fields.target_flashing = g_world_game_option_values[GAME_OPTION_TARGET_FLASHING];
        g_main_game_options.fields.show_unequippable_items
            = g_world_game_option_values[GAME_OPTION_SHOW_UNEQUIPPABLE_ITEMS];
        g_main_game_options.fields.max_equip_at_job_change
            = g_world_game_option_values[GAME_OPTION_MAX_EQUIP_AT_JOB_CHANGE];
        g_main_game_options.fields.sound_mode = g_world_game_option_values[GAME_OPTION_SOUND_MODE];
        world_thread_call_on_main_stack(g_world_game_option_values[GAME_OPTION_SOUND_MODE]);
        i++;
        column = g_main_game_options.fields.finger_cursor_repeat_speed;
        g_main_input_repeat_initial_delay = g_world_option_input_timing_profiles[column].initial_delay;
        g_main_input_repeat_period = g_world_option_input_timing_profiles[column].repeat_period;
        g_main_input_secondary_repeat_period = g_world_option_input_timing_profiles[column].secondary_repeat_period;
        g_main_menu_scroll_accel_delay = g_world_option_input_timing_profiles[column].scroll_accel_delay;
        g_main_menu_scroll_slow_step = g_world_option_input_timing_profiles[column].scroll_slow_step;
        g_main_menu_scroll_fast_step = g_world_option_input_timing_profiles[column].scroll_fast_step;
    }
    world_menu_free_memory(buffer);
    world_gfx_free_texture_grid_rect(&g_world_options_menu_title_rect);
    world_gfx_free_texture_grid_rect(&g_world_options_menu_value_rect);
    world_thread_wait_frames(1);
    g_world_menu_thread_result = 2;
    world_thread_set_parameters(g_world_thread_current_id + 1, 0, 0, 1);
    world_thread_exit_current();
}
