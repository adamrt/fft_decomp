/* Text record init, VRAM render record and the panel render thread. */

#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/wldcore.h"

/* Padded width and height, not position.
 * The mask is written 0xfffc (not ~3) to keep the target's `andi`. */
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
