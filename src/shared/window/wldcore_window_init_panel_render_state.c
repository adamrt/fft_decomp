#include "fft/wldcore.h"

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
