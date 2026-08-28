#include "fft/wldcore.h"
#include "fft/world.h"

/* Opens the paged text window: appends its four frame windows and the text
 * render record, lays the frames out around the padded text width, measures
 * the text and shows the first page.
 *
 * world_text_measure writes only the first two metrics; the array size
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

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    state->left_window_index = index;
    wldcore_window_init_record(index, state->priority, 8);
    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    state->middle_window_index = index;
    wldcore_window_init_record(index, state->priority, 9);
    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    state->right_window_index = index;
    wldcore_window_init_record(index, state->priority, 10);
    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    state->lower_window_index = index;
    wldcore_window_init_record(index, state->priority, 15);
    render_index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    state->render_record_index = render_index;
    g_wldcore_window_render_records[render_index].flags |= 0x100;
    g_wldcore_window_render_records[state->render_record_index].priority = state->priority;
    *(wldcore_point32_t*)&g_wldcore_window_render_records[state->render_record_index].base_x = state->base;

    padded_width = (state->text_width + 0x18) & 0xfffc;
    window_x = state->base.x + padded_width;
    window_y = state->base.y + state->rows_per_page * 16 + 16;
    if (padded_width % 8 == 4) {
        window_x -= 14;
    } else {
        window_x -= 12;
    }
    g_wldcore_window_records[state->left_window_index].x = window_x - 14;
    g_wldcore_window_records[state->left_window_index].y = window_y - 22;
    lower_y = window_y - 26;
    g_wldcore_window_records[state->middle_window_index].x = window_x;
    do {
        g_wldcore_window_records[state->middle_window_index].y = state->base.y + 8;
    } while (0);
    g_wldcore_window_records[state->right_window_index].x = window_x;
    g_wldcore_window_records[state->right_window_index].y = lower_y;
    g_wldcore_window_records[state->lower_window_index].x = window_x;
    g_wldcore_window_records[state->lower_window_index].y = lower_y;

    world_text_measure(&text_metrics[0], &text_metrics[1], world_text_find_entry(state->text_id));
    rows = text_metrics[1];
    last_page_row = (rows - 1) / state->rows_per_page * state->rows_per_page;
    state->page_start_row = 0;
    columns = text_metrics[0];
    state->scroll_state = 0;
    state->measured_rows = rows;
    state->measured_columns = columns;
    state->last_page_row = last_page_row;
    wldcore_text_render_scrollable_window_page(state, state->page_start_row);
}
