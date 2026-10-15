#include "fft/menu.h"
#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Opens the paged text window: appends its four frame windows and the text
 * render record, lays the frames out around the padded text width, measures
 * the text and shows the first page.
 *
 * text_measure writes only the first two metrics; the array size
 * reproduces the target's 0x60-byte frame. The one-trip do/while(0) around
 * the middle window's y store emits nothing: its loop notes are a
 * scheduling barrier that keeps the right/lower y subtraction in the
 * load delay slot of the middle window's first index load, as in the target.
 * Without it the scheduler sinks the subtraction to the right window's y
 * store. */
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
