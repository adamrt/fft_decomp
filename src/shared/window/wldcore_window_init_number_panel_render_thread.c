#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/wldcore.h"

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
