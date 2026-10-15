/* Render record images and VRAM uploads, panel render threads, cursor slots, scrollable text windows and the audio
 * command queue. */

#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

void wldcore_window_build_render_record_image(s32 index, RECT bounds, s32 coordinate_mode, u32* image) {
    point32_t* clut;
    CVECTOR* color;
    point32_t transformed;

    g_wldcore_window_render_records[index].flags
        &= ~(WORLD_RENDER_RECORD_8BIT_CLUT | WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN);
    g_wldcore_window_render_records[index].base.x = bounds.x - 0x80;
    g_wldcore_window_render_records[index].base.y = bounds.y - 0x78;
    g_wldcore_window_render_records[index].coordinate_mode = coordinate_mode;
    /* Struct view of clut_x/clut_y: as plain s32 stores they could alias the
     * stacked image argument, whose load the target schedules after them. */
    clut = &g_wldcore_window_render_records[index].clut;
    clut->y = 0x1E1;
    clut->x = 0;
    g_wldcore_window_render_records[index].bounds = bounds;
    wldcore_window_transform_point_for_mode(coordinate_mode, bounds, &transformed);
    g_wldcore_window_render_records[index].tpage = GetTPage(0, 0, transformed.x, transformed.y);
    color = &g_wldcore_window_render_records[index].color;
    color->b = color->g = color->r = 0x80;
    g_wldcore_window_render_records[index].anim_counter = 0;
    world_menu_build_window_frame_image(bounds.w, bounds.h, (u16*)image);
}

/* Transform a render position into the coordinate space selected by mode.
 *
 * The callee receives the adjacent position and dimensions registers as one
 * eight-byte aggregate. Dimensions are preserved by the ABI but unused here.
 */
void wldcore_window_transform_point_for_mode(s32 mode, RECT bounds, point32_t* output) {
    switch (mode) {
    case 0:
    case 1:
        output->x = bounds.x / 4 + 0x180 + mode * 64;
        output->y = bounds.y;
        break;
    case 2:
        output->x = (bounds.x / 4) + 0x240;
        output->y = bounds.y + 0x100;
        break;
    }
}

/* Upload a render record's window image at the VRAM position its coordinate
 * mode selects. The record's bounds are passed to the transform by value. */
void wldcore_window_load_image_record_to_vram(s32 index, u32* image) {
    point32_t transformed;
    RECT rect;

    wldcore_window_transform_point_for_mode(g_wldcore_window_render_records[index].coordinate_mode,
        g_wldcore_window_render_records[index].bounds, &transformed);
    rect.x = transformed.x;
    rect.y = transformed.y;
    rect.w = g_wldcore_window_render_records[index].bounds.w / 4;
    rect.h = g_wldcore_window_render_records[index].bounds.h;
    LoadImage(&rect, image);
}

/* `point` is passed by value: GCC spills the two 32-bit register values to their
 * home slots before narrowing them into the origin record. */
void window_display_text_entry_variant(s32 index, s32 text_id, point32_t point, u32* buffer) {
    glyph_blit_t origin;

    origin.rect.x = point.x;
    origin.rect.y = point.y;
    origin.stride = g_wldcore_window_render_records[index].bounds.w;
    menu_display_text_entry(text_id, (u8*)buffer, &origin);
}

void wldcore_text_get_padded_dimensions(s32 value, size32_t* output) {
    s16 maximum_line_width;
    s16 line_count;

    text_measure_pixels(&maximum_line_width, &line_count, text_find_entry(value));
    output->width = (maximum_line_width + 24) & 0xfffc;
    output->height = line_count * 16 + 16;
}

/* Build and upload a render record, optionally drawing its text label first. */
void window_init_text_record_variant(s32 index, RECT bounds, s32 coordinate_mode, s32 text_id, u32* image) {
    point32_t text_origin;

    wldcore_window_build_render_record_image(index, bounds, coordinate_mode, image);
    text_origin.x = 8;
    text_origin.y = 8;
    if (text_id != 0) {
        window_display_text_entry_variant(index, text_id, text_origin, image);
    }
    wldcore_window_load_image_record_to_vram(index, image);
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
}

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
