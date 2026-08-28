#include "fft/wldcore.h"
#include "fft/world.h"

/* Renders the page of a scrollable text window that starts at `row` and
 * clips its optional extra render record against the window top.
 *
 * The bounds are staged through word locals: storing the fields directly into
 * the u16 bounds narrows their loads to `lhu`, while the target reads words.
 * The substitution copy is a volatile store so reorg leaves the skip branch's
 * delay slot empty instead of pulling in the loop-tail source increment.
 * The final base copy is one point32 block copy (both loads precede both
 * stores); two field assignments reorder it. */
void wldcore_text_render_scrollable_window_page(wldcore_text_scrollable_window_t* state, s32 row) {
    wldcore_window_render_bounds16_t bounds;
    wldcore_point32_t point;
    s32 i;
    s32 offset;
    s32 top;
    s32 limit;
    s32 x;
    s32 y;
    s32 width;

    g_world_menu_text_param_22 = row;
    g_world_menu_text_param_24 = row + state->rows_per_page;
    x = state->image_x;
    bounds.position.x = x;
    y = state->image_y;
    bounds.position.y = y;
    width = (state->text_width + 0x18) & 0xFFFC;
    bounds.dimensions.x = width;
    bounds.dimensions.y = state->rows_per_page * 16 + 16;
    wldcore_window_build_render_record_image(state->render_record_index, bounds.position, bounds.dimensions,
        state->image_coordinate_mode, g_wldcore_window_image_buffer);
    for (i = 0; i < 2; i++) {
        if (state->text_substitutions[i] != -1) {
            ((volatile s32*)g_world_text_substitution_values)[i] = state->text_substitutions[i];
        }
    }
    if (state->extra_render_index != -1) {
        offset = row * 16;
        top = state->extra_base_y - offset;
        limit = state->base.y + 8;
        if (top >= limit) {
            g_wldcore_window_render_records[state->extra_render_index].base_y = top;
            g_wldcore_window_render_records[state->extra_render_index].y = state->extra_y;
            g_wldcore_window_render_records[state->extra_render_index].height = state->extra_height;
            g_wldcore_window_render_records[state->extra_render_index].flags &= ~0x10;
        } else if (top + state->extra_height >= limit) {
            g_wldcore_window_render_records[state->extra_render_index].base_y = limit;
            g_wldcore_window_render_records[state->extra_render_index].y = state->extra_y + offset;
            g_wldcore_window_render_records[state->extra_render_index].height = state->extra_height - offset;
            g_wldcore_window_render_records[state->extra_render_index].flags &= ~0x10;
        } else {
            g_wldcore_window_render_records[state->extra_render_index].flags |= 0x10;
        }
    }
    point.x = 8;
    point.y = 8;
    wldcore_menu_display_text_entry(state->render_record_index, state->text_id, point, g_wldcore_window_image_buffer);
    wldcore_window_load_image_record_to_vram(state->render_record_index, g_wldcore_window_image_buffer);
    *(wldcore_point32_t*)&g_wldcore_window_render_records[state->render_record_index].base_x = state->base;
}
