/* Window render and map opcodes. Its start is a boundary the binary proves; its end is a cut chosen by topic. */

#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Prepare a render record for an image that is already in VRAM.
 *
 * Caller flags replace bits 0x18; texture coordinates use doubled 4bpp units.
 */
void wldcore_window_init_vram_render_record(s32 index, RECT bounds, point32_t clut, s32 flags) {
    RECT* record_bounds;
    CVECTOR* rgb;

    g_wldcore_window_render_records[index].flags
        = (g_wldcore_window_render_records[index].flags & ~(WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN))
        | flags;
    g_wldcore_window_render_records[index].clut = clut;
    g_wldcore_window_render_records[index].tpage = GetTPage(0, 0, bounds.x, bounds.y);
    record_bounds = &g_wldcore_window_render_records[index].bounds;
    record_bounds->x = (bounds.x & 0x3F) * 2;
    record_bounds->y = bounds.y & 0xFF;
    record_bounds->w = bounds.w * 2;
    record_bounds->h = bounds.h;
    rgb = &g_wldcore_window_render_records[index].color;
    rgb->b = rgb->g = rgb->r = 0x80;
    g_wldcore_window_render_records[index].anim_counter = 0;
}

/* Initialize panel state and start its WORLD rendering thread. */
void wldcore_window_start_panel_render_thread(
    s32 thread_id, s32 extent, wldcore_window_panel_layout_t* layout, point32_t* origin) {
    wldcore_window_init_panel_render_state(thread_id, extent, layout, origin);
    thread_start(thread_id, world_screen_build_at_list);
    thread_set_parameters(thread_id, &g_wldcore_window_panel_render_state, 0, 0);
}

/* Initialize the panel renderer's shared geometry and data pointers.
 *
 * Width is rounded for its tiled image; stored origins are screen-relative.
 * Layout and origin values are loaded as full 32-bit values before narrowing to s16. */
void wldcore_window_init_panel_render_state(
    s32 unused, s32 extent, wldcore_window_panel_layout_t* layout, point32_t* origin) {
    s32 width = layout->text_width;
    s32 rows = layout->visible_rows;
    s32 text_width;

    g_wldcore_window_panel_label_state.row_count = rows;
    g_wldcore_window_panel_label_state.hidden_rows = extent - layout->visible_rows;
    g_wldcore_window_panel_label_state.x[0] = g_wldcore_window_panel_label_state.x[1] = 4;
    g_wldcore_window_panel_label_state.mode[0] = MENU_TEXT_COLUMN_TEXT;
    g_wldcore_window_panel_label_state.mode[1] = MENU_TEXT_COLUMN_HIDDEN;
    g_wldcore_window_panel_label_state.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
    g_wldcore_window_panel_label_state.text_ids[0] = g_wldcore_list_entry_values;
    g_wldcore_window_panel_label_state.text_colors[0] = g_wldcore_list_row_flags;
    text_width = (width + 0x18) & 0xFFFC;
    g_wldcore_window_panel_render_state.vram_origin.x = 0x200;
    g_wldcore_window_panel_render_state.vram_origin.y = 0x100;
    g_wldcore_window_panel_render_state.inner_width = text_width;
    g_wldcore_window_panel_render_state.inner_height = 0;
    g_wldcore_window_panel_render_state.window.x = origin->x - 0x80;
    g_wldcore_window_panel_render_state.window.y = origin->y - 0x78;
    g_wldcore_window_panel_render_state.window.w = text_width;
    g_wldcore_window_panel_render_state.window.h = 0;
    g_wldcore_window_panel_render_state.text_image.x_load = 0;
    g_wldcore_window_panel_render_state.text_image.y_load = 0;
    g_wldcore_window_panel_render_state.text_image.width = text_width;
    g_wldcore_window_panel_render_state.text_image.height = 0;
    g_wldcore_window_panel_render_state.text_image.x_screen_offset = 0;
    g_wldcore_window_panel_render_state.text_image.y_screen_offset = 0;
    g_wldcore_window_panel_render_state.text_id = 0;
    g_wldcore_window_panel_render_state.max_row_index = layout->visible_rows;
    g_wldcore_window_panel_render_state.thread_count = 0;
    g_wldcore_window_panel_render_state.row_actions = g_wldcore_panel_row_actions;
    g_wldcore_window_panel_render_state.header_id = 1;
    g_wldcore_window_panel_render_state.context = &g_wldcore_window_panel_label_state; /* a column layout */
    g_wldcore_window_panel_render_state.value = &g_wldcore_active_menu_value;
}

/* Save the active menu position in a persistent cursor slot. */
void wldcore_list_store_cursor_state(s32 index) {
    g_wldcore_menu_cursor_states[index].selected_index = g_wldcore_window_panel_render_state.selected_index;
    g_wldcore_menu_cursor_states[index].scroll_offset = g_wldcore_window_panel_render_state.select_text_table;
}

/* Clamp a saved cursor slot to the current list and restore it as active. */
void wldcore_list_clamp_cursor_state(s32 index, s32 count, s32 visible) {
    s32 end;

    if (count < g_wldcore_menu_cursor_states[index].selected_index + 1) {
        g_wldcore_menu_cursor_states[index].selected_index = count - 1;
        g_wldcore_menu_cursor_states[index].scroll_offset = count - visible;
    }
    end = g_wldcore_menu_cursor_states[index].scroll_offset;
    end += visible;
    if (count - end < 0) {
        g_wldcore_menu_cursor_states[index].scroll_offset = count - visible;
    }
    {
        s32 value;

        value = g_wldcore_menu_cursor_states[index].selected_index;
        g_wldcore_window_panel_render_state.selected_index = value;
        value = g_wldcore_menu_cursor_states[index].scroll_offset;
        g_wldcore_window_panel_render_state.select_text_table = value;
    }
}

void wldcore_list_clear_cursor_state(s32 index) {
    g_wldcore_menu_cursor_states[index].scroll_offset = 0;
    g_wldcore_menu_cursor_states[index].selected_index = 0;
}

/* Initialize and start a numeric panel renderer.
 *
 * The packed 32-bit value supplies two displayed 16-bit values; their magnitude selects
 * a one- through four-digit panel width before the optional label is measured.
 * `width` holds the digit count first and the panel's pixel width after. */
void wldcore_window_init_number_panel_render_thread(s32 thread_id, u32 packed_values, s32 text_id, point32_t* origin) {
    s16 text_width;
    s16 text_height;
    s32 width;
    s32 min = packed_values & 0xffff;
    s32 max = packed_values >> 16;
    s32 x;
    s32 y;
    s32 label_width;

    g_wldcore_window_panel_number_state.bounds.min = min;
    g_wldcore_window_panel_number_state.bounds.max = max;
    g_wldcore_window_panel_number_state._unknown_04[0] = -1;
    g_wldcore_window_panel_number_state._unknown_04[1] = -1;
    g_wldcore_window_panel_number_state.on_change = wldcore_window_number_panel_noop_callback;
    g_wldcore_list_entry_values[0] = g_wldcore_list_entry_values[1] = 0x7d0;
    if (min < 10 && max < 10) {
        width = 1;
    } else {
        width = 4;
        if (max < 1000) {
            width = 3;
        }
        if (max < 100) {
            width = 2;
        }
    }
    text_width = 0;
    if (text_id != 0) {
        text_measure_pixels(&text_width, &text_height, text_find_entry(text_id));
    }
    label_width = text_width + 0x18;
    width = (width * 6 + label_width) & 0xfffc;
    g_wldcore_window_panel_render_state.vram_origin.x = 0x200;
    g_wldcore_window_panel_render_state.vram_origin.y = 0x100;
    g_wldcore_window_panel_render_state.inner_width = width;
    g_wldcore_window_panel_render_state.inner_height = 0x20;
    x = origin->x;
    g_wldcore_window_panel_render_state.window.x = x;
    y = origin->y;
    g_wldcore_window_panel_render_state.window.y = y + 8;
    g_wldcore_window_panel_render_state.window.w = width;
    g_wldcore_window_panel_render_state.window.h = 0x20;
    g_wldcore_window_panel_render_state.text_image.x_load = 0;
    g_wldcore_window_panel_render_state.text_image.y_load = 0;
    g_wldcore_window_panel_render_state.text_image.width = width;
    g_wldcore_window_panel_render_state.text_image.height = 0x20;
    g_wldcore_window_panel_render_state.text_image.x_screen_offset = 0;
    g_wldcore_window_panel_render_state.text_image.y_screen_offset = 0;
    g_wldcore_window_panel_render_state.text_id = text_id;
    g_wldcore_window_panel_render_state.max_row_index = 0;
    g_wldcore_window_panel_render_state.thread_count = 0;
    g_wldcore_window_panel_render_state.row_actions = g_wldcore_list_entry_values;
    g_wldcore_window_panel_render_state.header_id = 0;
    g_wldcore_window_panel_render_state.context = &g_wldcore_window_panel_number_state; /* a value range */
    g_wldcore_window_panel_render_state.value = &g_wldcore_active_menu_value;
    g_wldcore_window_panel_render_state.selected_index = 0;
    g_wldcore_window_panel_render_state.select_text_table = 0;
    thread_start(thread_id, world_menu_labeled_number_entry_thread);
    thread_set_parameters(thread_id, (s32)&g_wldcore_window_panel_render_state, 0, 0);
}

/* The on_change callback of the number panel's value range: nothing to do. */
void wldcore_window_number_panel_noop_callback(void) {
}

void wldcore_text_init_scrollable_window(wldcore_text_scrollable_window_t* state) {
    s16 text_metrics[0x20];
    s32 index;
    s32 render_index;
    s32 padded_width;
    s32 window_x;
    s32 window_y;
    s32 lower_y;
    s32 rows;
    s32 columns;
    s32 last_page_row;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    state->left_window_index = index;
    wldcore_window_init_record(index, state->priority, WLDCORE_ANIM_SEQUENCE_TEXT_PAGE_PROMPT);
    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    state->middle_window_index = index;
    wldcore_window_init_record(index, state->priority, WLDCORE_ANIM_SEQUENCE_TEXT_SCROLL_UP);
    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    state->right_window_index = index;
    wldcore_window_init_record(index, state->priority, WLDCORE_ANIM_SEQUENCE_TEXT_SCROLL_DOWN);
    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    state->lower_window_index = index;
    wldcore_window_init_record(index, state->priority, WLDCORE_ANIM_SEQUENCE_TEXT_SCROLL_THUMB);
    render_index
        = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    state->render_record_index = render_index;
    g_wldcore_window_render_records[render_index].flags |= WORLD_RENDER_RECORD_POP_IN;
    g_wldcore_window_render_records[state->render_record_index].priority = state->priority;
    g_wldcore_window_render_records[state->render_record_index].base = state->base;

    padded_width = (state->text_width + 0x18) & 0xfffc;
    window_x = state->base.x + padded_width;
    window_y = state->base.y + state->rows_per_page * 16 + 16;
    if (padded_width % 8 == 4) {
        window_x -= 14;
    } else {
        window_x -= 12;
    }
    g_wldcore_window_records[state->left_window_index].position.x = window_x - 14;
    g_wldcore_window_records[state->left_window_index].position.y = window_y - 22;
    lower_y = window_y - 26;
    g_wldcore_window_records[state->middle_window_index].position.x = window_x;
    do {
        g_wldcore_window_records[state->middle_window_index].position.y = state->base.y + 8;
    } while (0);
    g_wldcore_window_records[state->right_window_index].position.x = window_x;
    g_wldcore_window_records[state->right_window_index].position.y = lower_y;
    g_wldcore_window_records[state->lower_window_index].position.x = window_x;
    g_wldcore_window_records[state->lower_window_index].position.y = lower_y;

    text_measure(&text_metrics[0], &text_metrics[1], text_find_entry(state->text_id));
    rows = text_metrics[1];
    last_page_row = (rows - 1) / state->rows_per_page * state->rows_per_page;
    state->page_start_row = 0;
    columns = text_metrics[0];
    state->scroll_state = WLDCORE_TEXT_SCROLL_PAGING;
    state->measured_rows = rows;
    state->measured_columns = columns;
    state->last_page_row = last_page_row;
    wldcore_text_render_scrollable_window_page(state, state->page_start_row);
}

/* Initializes window record `index`: sets flags 0x810, stores its animation
 * sequence and OT priority, and restarts the animation. */
void wldcore_window_init_record(s32 index, s32 priority, s32 sequence) {
    g_wldcore_window_records[index].sequence = sequence;
    g_wldcore_window_records[index].priority = priority;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_ANIMATE_WHILE_PAUSED;
}

/*
 * Per-frame scrollable text window handler.  State 0 waits for page, cancel or
 * confirm input, state 1 lets the player scroll one row at a time while confirm
 * is held, and state 2 auto-scrolls back to the page start.
 *
 * The state 2 page-start path repeats the update call instead of jumping to
 * the shared tail: its argument copy is then a no-op against the incoming $a0,
 * so only the other paths load $a0 (in their jump delay slots), as in the
 * target.
 */
s32 wldcore_text_is_window_finished(wldcore_text_scrollable_window_t* state) {
    if (state->scroll_state == WLDCORE_TEXT_SCROLL_RETURN) {
        if (state->scroll_row == state->page_start_row) {
            state->scroll_state = WLDCORE_TEXT_SCROLL_PAGING;
            wldcore_text_update_page_prompt_visibility(state);
            return 1;
        }
        state->scroll_timer += 1;
        if (state->scroll_timer <= 0) {
            return 1;
        }
        state->scroll_timer = 0;
        state->scroll_row += 1;
        wldcore_text_render_scrollable_window_page(state, state->scroll_row);
        return 1;
    }
    if (state->scroll_state == WLDCORE_TEXT_SCROLL_MANUAL) {
        if ((g_input_current_buttons & PSX_PAD_SQUARE) == 0) {
            state->scroll_state = WLDCORE_TEXT_SCROLL_RETURN;
            state->scroll_timer = 0;
            g_wldcore_window_records[state->middle_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[state->right_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[state->lower_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
            return 1;
        }
        if ((wldcore_input_update_repeat(PSX_PAD_UP) != 0) && (state->scroll_row != 0)) {
            state->scroll_row--;
        } else if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
            if (state->page_start_row > state->scroll_row) {
                state->scroll_row++;
            } else {
                return 1;
            }
        } else {
            return 1;
        }
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        wldcore_text_render_scrollable_window_page(state, state->scroll_row);
        wldcore_text_update_scroll_indicators(state);
        return 1;
    }
    if (state->scroll_state == WLDCORE_TEXT_SCROLL_PAGING) {
        if (g_wldcore_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
                state->page_start_row += state->rows_per_page;
            }
            if ((g_wldcore_input_new_button_presses & PSX_PAD_CROSS)
                || (state->page_start_row > state->last_page_row)) {
                g_wldcore_window_record_count -= 4;
                g_wldcore_window_render_record_count -= 1;
                g_wldcore_window_render_object_count -= 5;
                return 0;
            }
            wldcore_sound_play_effect(MAIN_SFX_TEXT_PAGE);
            wldcore_text_render_scrollable_window_page(state, state->page_start_row);
        } else if (g_wldcore_input_new_button_presses & PSX_PAD_SQUARE) {
            state->scroll_state = WLDCORE_TEXT_SCROLL_MANUAL;
            state->scroll_row = state->page_start_row;
            wldcore_text_update_scroll_indicators(state);
            g_wldcore_window_records[state->left_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
            return 1;
        }
        wldcore_text_update_page_prompt_visibility(state);
        return 1;
    }
    return 0;
}

/* Renders the page of a scrollable text window that starts at `row` and
 * clips its optional extra render record against the window top.
 *
 * The final base copy is one point32 block copy (both loads precede both
 * stores); two field assignments reorder it. */
void wldcore_text_render_scrollable_window_page(wldcore_text_scrollable_window_t* state, s32 row) {
    RECT bounds;
    point32_t point;
    s32 i;
    s32 offset;
    s32 top;
    s32 limit;

    g_menu_text_param_22 = row;
    g_menu_text_param_24 = row + state->rows_per_page;
    bounds.x = state->image_x;
    bounds.y = state->image_y;
    bounds.w = (state->text_width + 0x18) & 0xFFFC;
    bounds.h = state->rows_per_page * 16 + 16;
    wldcore_window_build_render_record_image(
        state->render_record_index, bounds, state->image_coordinate_mode, g_wldcore_window_image_buffer);
    for (i = 0; i < 2; i++) {
        if (state->text_substitutions[i] != -1) {
            g_text_substitution_values[i] = state->text_substitutions[i];
        }
    }
    if (state->extra_render_index != -1) {
        offset = row * 16;
        top = state->extra_base.y - offset;
        limit = state->base.y + 8;
        if (top >= limit) {
            g_wldcore_window_render_records[state->extra_render_index].base.y = top;
            g_wldcore_window_render_records[state->extra_render_index].bounds.y = state->extra_bounds.y;
            g_wldcore_window_render_records[state->extra_render_index].bounds.h = state->extra_bounds.h;
            g_wldcore_window_render_records[state->extra_render_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        } else if (top + state->extra_bounds.h >= limit) {
            g_wldcore_window_render_records[state->extra_render_index].base.y = limit;
            g_wldcore_window_render_records[state->extra_render_index].bounds.y = state->extra_bounds.y + offset;
            g_wldcore_window_render_records[state->extra_render_index].bounds.h = state->extra_bounds.h - offset;
            g_wldcore_window_render_records[state->extra_render_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        } else {
            g_wldcore_window_render_records[state->extra_render_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        }
    }
    point.x = 8;
    point.y = 8;
    window_display_text_entry_variant(state->render_record_index, state->text_id, point, g_wldcore_window_image_buffer);
    wldcore_window_load_image_record_to_vram(state->render_record_index, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[state->render_record_index].base = state->base;
}

/* Show the text-page prompt while more pages remain. */
void wldcore_text_update_page_prompt_visibility(wldcore_text_scrollable_window_t* state) {
    if (state->page_start_row < state->last_page_row) {
        g_wldcore_window_records[state->left_window_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    } else {
        g_wldcore_window_records[state->left_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    }
}

/*
 * Position the scroll-thumb window between the up and down arrow windows and
 * hide the arrows that are not usable at the current scroll position.
 */
void wldcore_text_update_scroll_indicators(wldcore_text_scrollable_window_t* state) {
    s32 top;
    s32 bottom;

    if (state->page_start_row == 0) {
        g_wldcore_window_records[state->middle_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[state->right_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[state->lower_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        return;
    }

    top = g_wldcore_window_records[state->right_window_index].position.y;
    bottom = g_wldcore_window_records[state->middle_window_index].position.y;
    g_wldcore_window_records[state->lower_window_index].position.y
        = bottom + (top - bottom - 0x12) * state->scroll_row / state->page_start_row + 0x12;
    g_wldcore_window_records[state->lower_window_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    if (state->scroll_row != 0) {
        g_wldcore_window_records[state->middle_window_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    } else {
        g_wldcore_window_records[state->middle_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    }
    if (state->scroll_row != state->page_start_row) {
        g_wldcore_window_records[state->right_window_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    } else {
        g_wldcore_window_records[state->right_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    }
}

void wldcore_sound_play_effect(s32 sound_id) {
    main_sound_play_sfx(sound_id);
}

/* Queue an audio command, servicing frame work while the queue is full.
 *
 * Accessing the count as a g_wldcore_audio_queue member is what makes GCC reload it
 * after each array store. */
void wldcore_sound_enqueue_audio_command(wldcore_audio_command_e command, s32 value) {
    while (g_wldcore_audio_queue.count >= WLDCORE_AUDIO_QUEUE_CAPACITY) {
        main_file_poll_load(&g_main_file_cd_state);
        wldcore_sound_process_audio_queue();
        VSync(0);
    }

    g_wldcore_audio_queue.commands[g_wldcore_audio_queue.count] = command;
    g_wldcore_audio_queue.values[g_wldcore_audio_queue.count] = value;
    g_wldcore_audio_queue.count++;
}

/* Services the audio command FIFO: dequeues the next command when idle and
 * runs the current one until it has to wait for loading or a fade.
 *
 * Commands: 1 loads and plays track `value & 0xff` into slot `value >> 8`,
 * 2 fades the current music out when the flag is set, 3 switches to the
 * current slot and fades in to `volume`, 4 unloads a slot (0 = current);
 * 0x10-0x12 wait for the load or the fade timer. The dispatch loop is a
 * backward goto: a `for (;;)` loop lets loop.c hoist the queue addresses
 * into saved registers, which the target never does. The if/else in case 4
 * keeps CSE from carrying the value's address across the join. Cases 2,
 * 0x10 and 0x11 jump into the default arm's reset as the target does; a
 * duplicated reset is cross-jumped into the last copy instead. */
void wldcore_sound_process_audio_queue(void) {
    s32 i;
    s32 value;
    s32 slot;
    s32 track;
    s32* slot_track;

next:
    switch (g_wldcore_audio_queue.current_command) {
    case WLDCORE_AUDIO_COMMAND_NONE:
        if (g_wldcore_audio_queue.count == 0) {
            return;
        }
        g_wldcore_audio_queue.current_command = g_wldcore_audio_queue.commands[0];
        g_wldcore_audio_queue.current_value = g_wldcore_audio_queue.values[0];
        for (i = 0; i < g_wldcore_audio_queue.count - 1; i++) {
            g_wldcore_audio_queue.commands[i] = g_wldcore_audio_queue.commands[i + 1];
            g_wldcore_audio_queue.values[i] = g_wldcore_audio_queue.values[i + 1];
        }
        g_wldcore_audio_queue.count--;
        break;
    case WLDCORE_AUDIO_COMMAND_PLAY_MUSIC:
        if (g_main_file_still_loading != 0) {
            return;
        }
        slot = (g_wldcore_audio_queue.current_value & 0xF00) >> 8;
        track = g_wldcore_audio_queue.current_value & 0xFF;
        slot_track = &g_wldcore_audio_queue.music.tracks[slot - 1];
        if (*slot_track == track) {
            g_wldcore_audio_queue.current_slot = slot;
            g_wldcore_audio_queue.current_command = WLDCORE_AUDIO_COMMAND_NONE;
            main_sound_switch_music_track(slot, 0, 0);
            return;
        }
        if (*slot_track != 0) {
            main_sound_unload_scenario_mus(slot);
        }
        if (main_sound_open_and_play_music(track, slot) != 0) {
            g_wldcore_audio_queue.current_command = WLDCORE_AUDIO_COMMAND_WAIT_LOAD;
            g_wldcore_audio_queue.current_slot = slot;
            *slot_track = track;
            return;
        }
        g_wldcore_audio_queue.current_command = WLDCORE_AUDIO_COMMAND_NONE;
        return;
    case WLDCORE_AUDIO_COMMAND_FADE_OUT:
        if ((g_wldcore_audio_queue.music.flags & WLDCORE_AUDIO_MUSIC_PLAYING) == 0) {
            goto clear_command;
        }
        main_sound_set_current_music_target(0, g_wldcore_audio_queue.current_value << 2);
        g_wldcore_audio_queue.current_command = WLDCORE_AUDIO_COMMAND_WAIT_FADE_OUT;
        return;
    case WLDCORE_AUDIO_COMMAND_FADE_IN:
        main_sound_switch_music_track(g_wldcore_audio_queue.current_slot, 0, 0);
        main_sound_set_current_music_target(g_wldcore_audio_queue.volume, g_wldcore_audio_queue.current_value << 2);
        g_wldcore_audio_queue.current_command = WLDCORE_AUDIO_COMMAND_WAIT_FADE_IN;
        return;
    case WLDCORE_AUDIO_COMMAND_UNLOAD:
        value = g_wldcore_audio_queue.current_value;
        if (value != 0) {
            slot = value;
        } else {
            slot = g_wldcore_audio_queue.current_slot;
        }
        main_sound_unload_scenario_mus(slot);
        if (slot == g_wldcore_audio_queue.current_slot) {
            g_wldcore_audio_queue.music.flags &= ~WLDCORE_AUDIO_MUSIC_PLAYING;
        }
        g_wldcore_audio_queue.music.tracks[slot - 1] = 0;
    default:
    clear_command:
        g_wldcore_audio_queue.current_command = WLDCORE_AUDIO_COMMAND_NONE;
        break;
    case WLDCORE_AUDIO_COMMAND_WAIT_LOAD:
        if (g_main_file_still_loading == 0) {
            goto clear_command;
        }
        return;
    case WLDCORE_AUDIO_COMMAND_WAIT_FADE_OUT:
        if (--g_wldcore_audio_queue.current_value != 0) {
            return;
        }
        g_wldcore_audio_queue.music.flags &= ~WLDCORE_AUDIO_MUSIC_PLAYING;
        main_sound_stop_current_music();
        goto clear_command;
    case WLDCORE_AUDIO_COMMAND_WAIT_FADE_IN:
        if (--g_wldcore_audio_queue.current_value != 0) {
            return;
        }
        g_wldcore_audio_queue.current_command = WLDCORE_AUDIO_COMMAND_NONE;
        g_wldcore_audio_queue.music.flags |= WLDCORE_AUDIO_MUSIC_PLAYING;
        break;
    }
    goto next;
}

/* Queue a music change: fade out what is playing, start `value` (a packed
 * slot and music id) and fade it in over 0x10 steps. */
void wldcore_sound_enqueue_music_start(s32 value) {
    if (g_wldcore_audio_queue.music.flags & WLDCORE_AUDIO_MUSIC_PLAYING) {
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x10);
    }
    wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_PLAY_MUSIC, value);
    wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_IN, 0x10);
}

/* Twin of wldcore_wait_for_file_load (0x800686c8) with the audio-queue depth
 * as the wait condition. The two 32-bit busy fields are read as g_wldcore_audio_queue members
 * rather than as separate scalars. */
void wldcore_sound_wait_for_queue_drain(void) {
    while (g_wldcore_audio_queue.count != 0 || g_wldcore_audio_queue.current_command != WLDCORE_AUDIO_COMMAND_NONE) {
        main_file_poll_load(&g_main_file_cd_state);
        wldcore_sound_process_audio_queue();
        VSync(0);
    }
}

s32 wldcore_script_process_conditional_set(s32 location, s32 action_mask) {
    s32 flags;
    u16 i;
    u16 entry;
    s32 offset;

    offset = g_wldcore_script_base[location] & 0xFFFE;
    g_wldcore_script_state.list = (u16*)((s32)g_wldcore_script_base - (-offset));
    for (i = 0; (entry = g_wldcore_script_state.list[i]) != 0; i++) {
        g_wldcore_script_state.data = (u16*)((s32)g_wldcore_script_base - (-(entry & 0xFFFE)));
        g_wldcore_script_state.ip = 0;
        g_wldcore_script_state.flags = 0;
        do {
            u16 ip = g_wldcore_script_state.ip;
            u16 op = g_wldcore_script_state.data[ip];
            g_wldcore_script_state.ip = ip + 1;
            g_wldcore_script_opcode_table[op]();
        } while ((g_wldcore_script_state.flags
                     & (WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_RESULT_CONDITION_FAILED))
            == 0);
        flags = g_wldcore_script_state.flags;
        if (flags & WLDCORE_SCRIPT_RESULT_ACTION_REACHED) {
            if (flags & action_mask) {
                return 1;
            }
        }
    }
    return 0;
}

void wldcore_opcode_map_nop(void) {
}

void wldcore_opcode_map_skip_1_operand(void) {
    u16* ip = &g_wldcore_script_state.ip;
    *ip += 1;
}

void wldcore_opcode_map_skip_2_operands(void) {
    u16* ip = &g_wldcore_script_state.ip;
    *ip += 2;
}

void wldcore_opcode_map_skip_3_operands(void) {
    u16* ip = &g_wldcore_script_state.ip;
    *ip += 3;
}

void wldcore_opcode_map_skip_4_operands(void) {
    u16* ip = &g_wldcore_script_state.ip;
    *ip += 4;
}

/* Reject the Var= condition when the script variable differs from the operand. */
void wldcore_opcode_map_check_var_equal(void) {
    s32 variable_id;
    s32 value;

    wldcore_script_read_operand_pair(&variable_id, &value);
    if (script_get_variable(variable_id) != value) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

void wldcore_script_read_operand_pair(s32* out1, s32* out2) {
    u16* ip = &g_wldcore_script_state.ip;
    u16 next_ip;

    *out1 = g_wldcore_script_state.data[*ip];
    next_ip = *ip + 1;
    *ip = next_ip;
    *out2 = g_wldcore_script_state.data[next_ip];
    *ip = *ip + 1;
}

/* Reject the Var>= condition when the script variable is below the operand. */
void wldcore_opcode_map_check_var_at_least(void) {
    s32 variable_id;
    s32 value;

    wldcore_script_read_operand_pair(&variable_id, &value);
    if (script_get_variable(variable_id) < value) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Var<= condition when the script variable is above the operand. */
void wldcore_opcode_map_check_var_at_most(void) {
    s32 variable_id;
    s32 value;

    wldcore_script_read_operand_pair(&variable_id, &value);
    if (script_get_variable(variable_id) > value) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the HasUnit condition when no roster unit has the operand's sprite set. */
void wldcore_opcode_map_check_roster_has_sprite_set(void) {
    s32 i;
    s32 sprite_set;
    u16* ipp;
    u16 ip;
    party_unit_t* unit;
    s32* flags;

    ipp = &g_wldcore_script_state.ip;
    ip = *ipp;
    sprite_set = g_wldcore_script_state.data[ip];
    *ipp = ip + 1;
    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = wldcore_get_party_unit(i);
        if (unit->party_id != PARTY_ID_NONE) {
            if (unit->sprite_set == sprite_set) {
                break;
            }
        }
    }
    if (i == PARTY_ROSTER_SLOT_COUNT) {
        flags = &g_wldcore_script_state.flags;
        *flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

void wldcore_opcode_map_skip_1_operand_0x0c(void) {
    u16* ip = &g_wldcore_script_state.ip;
    *ip += 1;
}

void wldcore_opcode_map_skip_1_operand_0x0d(void) {
    u16* ip = &g_wldcore_script_state.ip;
    *ip += 1;
}

/* Reject the Gil>= condition when current war funds are below the operand. */
void wldcore_opcode_map_check_war_funds_at_least(void) {
    u16* ip = &g_wldcore_script_state.ip;
    u16 idx = *ip;
    u16 value = g_wldcore_script_state.data[idx];
    *ip = idx + 1;
    if (script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) < value) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Gil<= condition when current war funds are above the operand. */
void wldcore_opcode_map_check_war_funds_at_most(void) {
    u16* ip = &g_wldcore_script_state.ip;
    u16 idx = *ip;
    s32 value = g_wldcore_script_state.data[idx];
    *ip = idx + 1;
    if (value < script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS)) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Date>= condition when the current date precedes the operand. */
void wldcore_opcode_map_check_date_on_or_after(void) {
    s32 month, day, current_month, current_day;
    s32* flags;

    wldcore_script_read_operand_pair_and_date(&month, &day, &current_month, &current_day);
    if ((current_month < month) || ((current_month == month) && (current_day < day))) {
        flags = &g_wldcore_script_state.flags;
        *flags = *flags | WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

void wldcore_script_read_operand_pair_and_date(
    s32* out_month, s32* out_day, s32* out_current_month, s32* out_current_day) {
    u16* ipp;
    u16 ip;

    ipp = &g_wldcore_script_state.ip;
    *out_month = g_wldcore_script_state.data[*ipp];
    ip = *ipp + 1;
    *ipp = ip;
    *out_day = g_wldcore_script_state.data[ip];
    *ipp = *ipp + 1;
    *out_current_month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    *out_current_day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
}

/* Reject the Date<= condition when the current date follows the operand. */
void wldcore_opcode_map_check_date_on_or_before(void) {
    s32 month, day, current_month, current_day;
    s32* flags;

    wldcore_script_read_operand_pair_and_date(&month, &day, &current_month, &current_day);
    if ((current_month > month) || ((current_month == month) && (current_day > day))) {
        flags = &g_wldcore_script_state.flags;
        *flags = *flags | WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Casualties>= condition when the current count is below the operand. */
void wldcore_opcode_map_check_casualties_at_least(void) {
    u16* ip = &g_wldcore_script_state.ip;
    u16 idx = *ip;
    u16 value = g_wldcore_script_state.data[idx];
    *ip = idx + 1;
    if (script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES) < value) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Casualties<= condition when the current count is above the operand. */
void wldcore_opcode_map_check_casualties_at_most(void) {
    u16* ip = &g_wldcore_script_state.ip;
    u16 idx = *ip;
    s32 value = g_wldcore_script_state.data[idx];
    *ip = idx + 1;
    if (value < script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES)) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Stage the two 32-bit fields of the event identifier for a deferred LoadEvent request. */
void wldcore_opcode_map_request_load_event(void) {
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.args[1] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_LOAD_EVENT;
}

/* Stage four variable/value pairs for a deferred Choice request. */
void wldcore_opcode_map_request_choice(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        g_wldcore_script_state.choice_vars[i] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
        g_wldcore_script_state.args[i] = g_wldcore_script_state.data[++g_wldcore_script_state.ip];
        g_wldcore_script_state.ip++;
    }
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_CHOICE;
}

/* World Map Script 0x1C - SetVar: read next two script args and set a variable. */
void wldcore_opcode_map_set_var(void) {
    s32 variable_id;
    s32 value;

    wldcore_script_read_operand_pair(&variable_id, &value);
    script_set_variable(variable_id, value);
}

void wldcore_opcode_map_request_text(void) {
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_TEXT;
}

/* Stage the three operands for a deep-dungeon entry request.
 *
 * Each operand advances the script IP before the request becomes ready.
 */
void wldcore_opcode_map_request_deep_dungeon_entry(void) {
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.args[1] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.args[2] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_DEEP_DUNGEON_ENTRY;
}

/* Stage the two operands for the deferred SetVar2 request.
 *
 * This opcode only prepares the request; a later dispatcher performs it.
 */
void wldcore_opcode_map_request_set_var_2(void) {
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.args[1] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_SET_VAR_2;
}

/* Stage the two operands for a deferred DrawPath request.
 *
 * The request flag is published after both operands and IP updates.
 */
void wldcore_opcode_map_request_draw_path(void) {
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.args[1] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_DRAW_PATH;
}

void wldcore_opcode_map_request_erase_path(void) {
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.args[1] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_ERASE_PATH;
}

/* Stage the location ID for a deferred DrawLocation request. */
void wldcore_opcode_map_request_draw_location(void) {
    g_wldcore_script_flags |= WLDCORE_SCRIPT_REQUEST_DRAW_LOCATION;
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
}

/* Stage the location ID for a deferred EraseLocation request. */
void wldcore_opcode_map_request_erase_location(void) {
    g_wldcore_script_flags |= WLDCORE_SCRIPT_REQUEST_ERASE_LOCATION;
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
}

/* Resolve the script-selected location and focus the map on its window entry.
 */
void wldcore_opcode_map_focus_location(void) {
    u16 ip = g_wldcore_script_state.ip;
    u16* data = g_wldcore_script_state.data;
    u16 entry;
    s32 index;

    entry = data[ip];
    g_wldcore_script_state.args[0] = entry;
    if (entry == 0xff) {
        index = g_wldcore_map_projection_state.marker.kind;
        g_wldcore_proposition_selected_entry = index + 1;
    } else {
        index = entry;
        g_wldcore_proposition_selected_entry = index + 1;
    }
    if (wldcore_map_is_point_outside_projection_bounds(&g_wldcore_map_dots[index].render.position) != 0) {
        s32* flags = &g_wldcore_script_state.flags;

        *flags |= WLDCORE_SCRIPT_ACTION_FOCUS_LOCATION;
    }
    {
        s32* flags = &g_wldcore_script_state.flags;

        *flags |= WLDCORE_SCRIPT_RESULT_ACTION_REACHED;
    }
}

void wldcore_opcode_map_check_ramza_brave_at_least(void) {
    party_unit_t* party;
    u16 ip;
    s32 val;

    party = wldcore_get_party_unit(wldcore_get_ramza_roster_index());
    ip = g_wldcore_script_state.ip;
    val = g_wldcore_script_state.data[ip];
    g_wldcore_script_state.ip = ip + 1;
    if (party->bravery < val) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}
