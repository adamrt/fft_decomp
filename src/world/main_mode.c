/* The main menu mode and game options. */

#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Options-menu values: g_menu_option_values holds the 13 options unpacked from
 * g_main_game_options plus an all-unchanged flag at [13]; g_menu_option_reference_values holds the
 * values they are compared against. */

/* Unpacks the configured options into the options-menu value table, then runs
 * main-menu mode 0 (options thread in the previous slot), 1 or 2. */
void world_menu_run_main_mode(s32 mode) {
    menu_entry_t* entry;
    game_options_fields_t options;
    s32 i;
    s32 last_mode;

    entry = (menu_entry_t*)thread_get_current_parameter_1();
    options = g_main_game_options.fields;
    g_menu_option_values[GAME_OPTION_CURSOR_MOVEMENT] = options.cursor_movement;
    g_menu_option_values[GAME_OPTION_CURSOR_REPEAT_SPEED] = options.cursor_repeat_speed;
    g_menu_option_values[GAME_OPTION_MULTI_HEIGHT_CURSOR_SPEED] = options.multi_height_cursor_speed;
    g_menu_option_values[GAME_OPTION_FINGER_CURSOR_REPEAT_SPEED] = options.finger_cursor_repeat_speed;
    g_menu_option_values[GAME_OPTION_MESSAGE_DISPLAY_SPEED] = options.message_display_speed;
    g_menu_option_values[GAME_OPTION_NAVIGATION_MESSAGES] = options.navigation_messages;
    g_menu_option_values[GAME_OPTION_ABILITY_NAMES] = options.ability_names;
    g_menu_option_values[GAME_OPTION_EFFECT_MESSAGES] = options.effect_messages;
    g_menu_option_values[GAME_OPTION_DISPLAY_GAINED_EXP_JP] = options.display_gained_exp_jp;
    g_menu_option_values[GAME_OPTION_TARGET_FLASHING] = options.target_flashing;
    g_menu_option_values[GAME_OPTION_SHOW_UNEQUIPPABLE_ITEMS] = options.show_unequippable_items;
    g_menu_option_values[GAME_OPTION_MAX_EQUIP_AT_JOB_CHANGE] = options.max_equip_at_job_change;
    g_menu_option_values[GAME_OPTION_SOUND_MODE] = options.sound_mode;
    g_menu_option_values[GAME_OPTION_UNCHANGED] = 1;
    for (i = 0; i < GAME_OPTION_COUNT; i++) {
        if (g_menu_option_values[i] != g_menu_option_reference_values[i]) {
            g_menu_option_values[GAME_OPTION_UNCHANGED] = 0;
        }
    }
    g_menu_modal_open = 0;
    g_menu_hide_numeric_values = 0;
    /* The target keeps this constant in a callee-saved register across the
     * mode 0 and mode 1 calls, so it is its own variable. */
    last_mode = OPTION_MENU_DEAD_UNIT;
    if (mode == OPTION_MENU_GAME_OPTIONS) {
        script_pulse_tutorial_wait_value(0xFD);
        thread_start(g_thread_current_id - 1, menu_run_game_options_variant);
        entry->select_text_table = 1;
        entry->context = &g_menu_option_entry_table;
        thread_set_parameters(g_thread_current_id - 1, (s32)entry, 0, 0);
        thread_wait_until_inactive(g_thread_current_id - 1);
        thread_exit_current();
    }
    if (mode == OPTION_MENU_AT_LIST) {
        script_pulse_tutorial_wait_value(0xF9);
        menu_start_at_list_thread();
    }
    if (mode == last_mode) {
        menu_start_dead_unit_threads();
    }
}

/*
 * Options-menu thread started by world_menu_run_main_mode (mode 0) with
 * entry->context = &g_menu_option_entry_table.
 *
 * Scrolls the two-column option list like menu_run_scrolling_ability_list_thread_variant, shows the
 * selected option's value text (g_menu_option_value_text_id_bases[i] + g_menu_option_values[i]),
 * starts menu_run_option_choice on confirm, and each frame packs the option
 * values back into g_main_game_options, applies the sound mode on the main
 * stack and reloads the input-repeat timings for the finger-cursor speed.
 *
 * The do/while (0) around the thread lookup is a scheduling barrier: its
 * loop notes keep the input-1 spill and the entry load ahead of the
 * row-count loads, as in the target.
 */
void menu_run_game_options_variant(void) {
    image_load_parameters_t image_parameters;
    RECT image_rect;
    RECT clear;
    RECT texture_window;
    RECT value_rect;
    s32 row_offset;
    s32 ring;
    s32 cursor;
    thread_t* thread;
    u8* buffer;
    u32* input0;
    u32* input1;
    s32 accum;
    s32 timer;
    menu_entry_t* entry;
    options_menu_page_t* page;
    options_menu_page_t* base;
    menu_text_layout_t* layout;
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
    input0 = script_get_controller_input_pointer(0);
    input1 = script_get_controller_input_pointer(1);
    do {
        thread = &g_threads[g_thread_current_id];
        entry = (menu_entry_t*)thread->function_parameter_1;
    } while (0);
    page = g_menu_option_pages;
    layout = g_menu_option_entry_table;
    entry->window.h = layout->row_count * 16 + 0x10;
    entry->inner_height = layout->row_count * 16 + 0x10;
    entry->text_image.height = layout->row_count * 16 + 0x10;
    image_parameters.x_screen_offset = 8;
    image_parameters.x_load = 0;
    image_parameters.y_load = 0;
    image_parameters.y_screen_offset = 0xA;
    image_parameters.width = entry->window.w - 0x20;
    image_parameters.height = entry->window.h - 0xE;
    menu_build_window_sprites(&image_rect, entry, &page->icons);
    buffer = menu_alloc_memory(0x1E0);
    g_menu_option_pages[0].icons.base.sprites[3].y0 -= 2;
    clear.x = entry->vram_origin.x;
    clear.y = 0xFE;
    clear.w = (s16)image_parameters.width / 4;
    clear.h = 2;
    ClearImage(&clear, 0, 0, 0);
    gfx_set_draw_mode_for_texture_page(&page->icon_mode, MENU_TEXTURE_PAGE_FRAME);
    gfx_set_draw_mode_for_texture_page(&page->text_mode, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    texture_window.x = 0;
    texture_window.y = 0;
    texture_window.w = 0x100;
    texture_window.h = 0x100;
    SetDrawMode(&page->window_mode, 0, 0, GetTPage(0, 0, entry->vram_origin.x & 0xFFC0, entry->vram_origin.y & 0xFF00),
        &texture_window);
    arrow = page->arrows;
    for (i = 0, arrow_u = 0xD8; i < 2; i++) {
        menu_init_sprt(&arrow[i]);
        page->arrows[i].clut = 0x7D7C;
        arrow[i].w = 8;
        arrow[i].h = 0x10;
        arrow[i].u0 = arrow_u;
        arrow[i].v0 = 0;
        arrow[i].x0 = entry->window.x + entry->window.w - 0xF;
        arrow_y = i * (entry->window.h - 0x20) + 8;
        arrow[i].y0 = entry->window.y + arrow_y;
        arrow_u += 8;
    }
    base = g_menu_option_pages;
    base->arrows[0].y0 += 4;
    menu_init_sprt(&base->thumb);
    base->thumb.u0 = 0x10;
    base->thumb.v0 = 0x10;
    base->thumb.w = 8;
    base->thumb.h = 8;
    base->thumb.clut = 0x7D7C;
    menu_init_sprt(&base->frame);
    gfx_init_image_loading(&base->frame, &entry->vram_origin, &entry->window, &image_parameters);
    menu_init_sprite_array(base->arrow_marks, 2, 0x7C3C);
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
    image = g_menu_render_buffer;
    delta = 0;
    i = 0;
    accum = 0;
    timer = 0;
    menu_clear_render_buffer(image, entry->inner_width * entry->inner_height / 2);
    menu_draw_text_columns(entry, &row_offset, image);
    ring = 0;
    image_rect.w = g_menu_text_state.pen.stride >> 2;
    LoadImage(&image_rect, (u32*)image);
    thread_wait_frames(1);
    menu_init_scaled_draw_area_packets(&page->title_area);
    menu_init_scaled_draw_area_packets(&page->value_area);
    menu_init_sprt(&base->title);
    text_configure_sprite_vram(&g_menu_option_title_image_rect, entry->inner_width, 0x2C, &base->title, 0);
    image = menu_build_and_upload_window_frame_image(
        entry->inner_width, 0x2C, &g_menu_option_title_image_rect, MENU_WINDOW_FRAME_ALTERNATE_BUILD);
    base->title.x0 = entry->window.x;
    base->title.y0 = entry->window.y + entry->window.h + 1;
    g_menu_text_state.pen.stride = entry->inner_width;
    menu_set_text_origin(0x18, 0xE);
    menu_display_text_entry(0x502B, image, &g_menu_text_state.pen);
    LoadImage(&g_menu_option_title_image_rect, (u32*)image);
    thread_wait_frames(1);
    menu_free_memory(image);
    value_rect.x = base->title.x0;
    value_rect.y = base->title.y0;
    value_rect.w = base->title.w;
    value_rect.h = base->title.h;
    mem_copy_bytes_nonempty(&g_menu_option_entry_panel_rect, &entry->window.x, sizeof(g_menu_option_entry_panel_rect));
    thread_start(g_thread_current_id + 1, menu_window_frame_thread);
    g_menu_option_entry_panel_rect.h += 0xC;
    g_menu_option_entry_panel_rect.y -= 2;
    thread_set_parameters(g_thread_current_id + 1, (s32)&g_menu_option_entry_panel_rect, 0, 0);
    thread_yield();
    menu_init_sprt(&base->value);
    text_configure_sprite_vram(&g_menu_option_value_image_rect, 0x3C, 0x10, &base->value, 0);
    base->value.x0 = entry->window.x + 0x5C;
    base->value.y0 = entry->window.y + entry->window.h + 0xF;
    menu_clear_render_buffer(buffer, 0x1E0);
    g_menu_text_state.pen.stride = 0x3C;
    menu_set_text_origin(0, 0);
    menu_display_text_entry(0x5021, buffer, &g_menu_text_state.pen);
    LoadImage(&g_menu_option_value_image_rect, (u32*)buffer);
    mem_copy_bytes_nonempty(&base[1], base, sizeof(options_menu_page_t));
    for (;;) {
        page = &g_menu_option_pages[i & 1];
        if (delta == 0 && menu_can_accept_input() != 0) {
            if ((*input0 & PSX_PAD_UP) && (!(*input1 & PSX_PAD_SQUARE) || g_menu_input_mode != MENU_INPUT_PLAYER)) {
                if (cursor != 0) {
                    cursor--;
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset != 0) {
                    delta = -1;
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                } else if (layout->hidden_rows == 0) {
                    cursor = layout->row_count - 1;
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                }
            } else if ((*input0 & PSX_PAD_DOWN)
                && (!(*input1 & PSX_PAD_SQUARE) || g_menu_input_mode != MENU_INPUT_PLAYER)) {
                if (cursor != layout->row_count - 1) {
                    cursor++;
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset != layout->hidden_rows) {
                    delta++;
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset == 0) {
                    cursor = 0;
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
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
                image = g_menu_render_buffer;
                image_rect.h = 0x10;
                menu_clear_render_buffer(image, entry->inner_width * 8);
                g_menu_text_state.pen.stride = entry->inner_width;
                for (column = 0; column < 2; column++) {
                    if (layout->text_ids[column] != (s16*)-1) {
                        index = cursor + row_offset + delta;
                        menu_set_text_origin(layout->x[column], 0);
                        g_menu_text_palette_offset = layout->text_colors[column][index];
                        if (layout->mode[column] == MENU_TEXT_COLUMN_TEXT) {
                            menu_display_text_entry(layout->text_ids[column][index], image, &g_menu_text_state.pen);
                        } else {
                            menu_display_number_entry(
                                layout->text_ids[column][index], 2, image, &g_menu_text_state.pen);
                        }
                    }
                }
                LoadImage(&image_rect, (u32*)image);
            }
            if (*input0 & PSX_PAD_CIRCLE) {
                g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
                if (menu_can_accept_input() != 0) {
                    *input0 = 0;
                    index = cursor + row_offset + delta;
                    thread_start(g_thread_current_id - 1, menu_run_option_choice);
                    thread_set_parameters(g_thread_current_id - 1, index, 0, 0);
                }
            }
            if (*input0 & PSX_PAD_CROSS) {
                menu_handle_cancel_input(entry);
            }
        }
        if (g_menu_input_lock_mode == 0) {
            index = cursor + row_offset + delta;
            menu_clear_render_buffer(buffer, 0x1E0);
            g_menu_text_state.pen.stride = 0x3C;
            menu_set_text_origin(0, 0);
            menu_display_text_entry(
                g_menu_option_value_text_id_bases[index] + g_menu_option_values[index], buffer, &g_menu_text_state.pen);
            LoadImage(&g_menu_option_value_image_rect, (u32*)buffer);
        }
        thread_yield();
        if (menu_should_close_thread(input0) != 0) {
            break;
        }
        if (delta == 0 && menu_can_accept_input() != 0) {
            menu_redraw_text_page_on_scroll(entry, &row_offset, &ring, &cursor);
            ClearImage(&clear, 0, 0, 0);
        }
        menu_select_icon_cluts(&page->icons.base);
        menu_update_icon_cursor_sprites(entry, &page->icons.base, i, cursor);
        page->icons.base.sprites[1].y0 += 2;
        page->icons.base.sprites[2].y0 += 2;
        entry->selected_index = row_offset + cursor;
        if ((g_script_event_speed == 1 && !(script_get_event_initial_repeat_counter() & 1))
            || g_script_event_speed == 2) {
            if (timer < g_main_menu_scroll_accel_delay) {
                step = g_main_menu_scroll_slow_step * 2;
            } else {
                step = g_main_menu_scroll_fast_step * 2;
            }
            accum += delta * step;
        }
        timer += g_script_event_speed;
        frame = &page->frame;
        if (accum < -0xF || accum > 0xF) {
            ring += delta;
            row_offset += delta;
            repeat = script_get_event_initial_repeat_counter();
            delta = 0;
            accum = 0;
            if (repeat >= g_main_input_repeat_initial_delay / g_script_event_speed) {
                *input0 |= *input1 & (PSX_PAD_UP | PSX_PAD_DOWN);
            }
        }
        page->frame.v0 = ring * 16 + accum - 2;
        menu_select_sprite_clut(&page->title);
        menu_select_sprite_clut(&page->value);
        menu_select_sprite_clut(frame);
        gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[3]);
        gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[1]);
        gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[2]);
        gfx_draw_or_append_gpu_primitive(&page->icons.base.draw_mode);
        if (layout->hidden_rows != 0 && menu_can_accept_input() != 0) {
            if (row_offset != 0) {
                gfx_draw_or_append_gpu_primitive(&page->arrows[0]);
                gfx_draw_or_append_gpu_primitive(&page->arrow_marks[0]);
            }
            if (row_offset != layout->hidden_rows) {
                gfx_draw_or_append_gpu_primitive(&page->arrows[1]);
                gfx_draw_or_append_gpu_primitive(&page->arrow_marks[1]);
            }
            span = page->arrows[1].y0;
            top = page->arrows[0].y0 + page->arrows[0].h;
            span -= top;
            page->thumb.x0 = page->arrows[0].x0;
            page->thumb.y0 = top + span * entry->selected_index / (layout->row_count + layout->hidden_rows) - 2;
            gfx_draw_or_append_gpu_primitive(&page->thumb);
        }
        if ((thread - 1)->is_running == 0) {
            thread_set_parameters(g_thread_current_id + 1, (s32)&entry->window.x, 0, 0);
        } else {
            thread_set_parameters(g_thread_current_id + 1, (s32)&entry->window.x, 1, 0);
        }
        gfx_build_scaled_draw_area_pair_swapped(&page->title_area, &entry->window, i, g_active_graphics_buffer_index);
        gfx_build_scaled_draw_area_pair_swapped(&page->value_area, &value_rect, i, g_active_graphics_buffer_index);
        gfx_draw_or_append_gpu_primitive(&page->title_area.areas[1]);
        gfx_draw_or_append_gpu_primitive(&page->icon_mode);
        gfx_draw_or_append_gpu_primitive(frame);
        gfx_draw_or_append_gpu_primitive(&page->window_mode);
        gfx_draw_or_append_gpu_primitive(&page->title_area.areas[0]);
        gfx_draw_or_append_gpu_primitive(&page->value_area.areas[1]);
        gfx_draw_or_append_gpu_primitive(&page->value);
        gfx_draw_or_append_gpu_primitive(&page->title);
        gfx_draw_or_append_gpu_primitive(&page->text_mode);
        gfx_draw_or_append_gpu_primitive(&page->value_area.areas[0]);
        g_thread_call_target = (void (*)(void))suzuki_sound_set_type;
        g_main_game_options.fields.cursor_movement = g_menu_option_values[GAME_OPTION_CURSOR_MOVEMENT];
        g_main_game_options.fields.cursor_repeat_speed = g_menu_option_values[GAME_OPTION_CURSOR_REPEAT_SPEED];
        g_main_game_options.fields.multi_height_cursor_speed
            = g_menu_option_values[GAME_OPTION_MULTI_HEIGHT_CURSOR_SPEED];
        g_main_game_options.fields.finger_cursor_repeat_speed
            = g_menu_option_values[GAME_OPTION_FINGER_CURSOR_REPEAT_SPEED];
        g_main_game_options.fields.message_display_speed = g_menu_option_values[GAME_OPTION_MESSAGE_DISPLAY_SPEED];
        g_main_game_options.fields.navigation_messages = g_menu_option_values[GAME_OPTION_NAVIGATION_MESSAGES];
        g_main_game_options.fields.ability_names = g_menu_option_values[GAME_OPTION_ABILITY_NAMES];
        g_main_game_options.fields.effect_messages = g_menu_option_values[GAME_OPTION_EFFECT_MESSAGES];
        g_main_game_options.fields.display_gained_exp_jp = g_menu_option_values[GAME_OPTION_DISPLAY_GAINED_EXP_JP];
        g_main_game_options.fields.target_flashing = g_menu_option_values[GAME_OPTION_TARGET_FLASHING];
        g_main_game_options.fields.show_unequippable_items = g_menu_option_values[GAME_OPTION_SHOW_UNEQUIPPABLE_ITEMS];
        g_main_game_options.fields.max_equip_at_job_change = g_menu_option_values[GAME_OPTION_MAX_EQUIP_AT_JOB_CHANGE];
        g_main_game_options.fields.sound_mode = g_menu_option_values[GAME_OPTION_SOUND_MODE];
        thread_call_on_main_stack(g_menu_option_values[GAME_OPTION_SOUND_MODE]);
        i++;
        column = g_main_game_options.fields.finger_cursor_repeat_speed;
        g_main_input_repeat_initial_delay = g_menu_option_input_repeat_timings[column].initial_delay;
        g_main_input_repeat_period = g_menu_option_input_repeat_timings[column].repeat_period;
        g_main_input_secondary_repeat_period = g_menu_option_input_repeat_timings[column].secondary_repeat_period;
        g_main_menu_scroll_accel_delay = g_menu_option_input_repeat_timings[column].scroll_accel_delay;
        g_main_menu_scroll_slow_step = g_menu_option_input_repeat_timings[column].scroll_slow_step;
        g_main_menu_scroll_fast_step = g_menu_option_input_repeat_timings[column].scroll_fast_step;
    }
    menu_free_memory(buffer);
    gfx_free_texture_rect(&g_menu_option_title_image_rect);
    gfx_free_texture_rect(&g_menu_option_value_image_rect);
    thread_wait_frames(1);
    g_world_menu_thread_result = 2;
    thread_set_parameters(g_thread_current_id + 1, 0, 0, 1);
    thread_exit_current();
}
