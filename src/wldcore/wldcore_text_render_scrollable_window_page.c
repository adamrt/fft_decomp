#include "fft/wldcore.h"

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
