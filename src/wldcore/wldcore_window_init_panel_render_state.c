#include "fft/wldcore.h"

/* Initialize the panel renderer's shared geometry and data pointers.
 *
 * Width is rounded for its tiled image; stored origins are screen-relative.
 */
void wldcore_window_init_panel_render_state(
    s32 unused, s32 extent, wldcore_point32_t* dimensions, wldcore_point32_t* origin) {
    s32 rounded_width = dimensions->x;
    s32 height = dimensions->y;
    s32 height_again;
    s32 origin_coordinate;
    s32 final_height;
    /* Pins required: unpinned, the loaded value and data pointer move out of v0 into t0/t1. */
    register s32 value __asm__("$2");
    register void* data_pointer __asm__("$2");
    world_menu_text_layout_t* label;

    label = &g_wldcore_window_panel_label_state;
    label->row_count = height;
    height_again = dimensions->y;
    value = 4;
    label->x[1] = value;
    label->x[0] = value;
    value = 2;
    label->mode[1] = value;
    label->mode[2] = value;
    value = (s32)g_wldcore_list_entry_values;
    label->text_ids[0] = (s16*)value;
    value = (s32)g_wldcore_list_row_flags;
    label->text_colors[0] = (s16*)value;
    value = 0x200;
    g_wldcore_window_panel_render_state.vram_x = value;
    value = 0x100;
    rounded_width = (rounded_width + 0x18) & 0xFFFC;
    label->mode[0] = 0;
    g_wldcore_window_panel_render_state.vram_y = value;
    g_wldcore_window_panel_render_state.inner_width = rounded_width;
    g_wldcore_window_panel_render_state.inner_height = 0;
    extent -= height_again;
    label->hidden_rows = extent;
    origin_coordinate = origin->x;
    g_wldcore_window_panel_render_state.window_x = origin_coordinate - 0x80;
    origin_coordinate = origin->y;
    g_wldcore_window_panel_render_state.window_width = rounded_width;
    g_wldcore_window_panel_render_state.window_height = 0;
    g_wldcore_window_panel_render_state.unknown_10[0] = 0;
    g_wldcore_window_panel_render_state.unknown_10[1] = 0;
    g_wldcore_window_panel_render_state.overall_width = rounded_width;
    g_wldcore_window_panel_render_state.overall_height = 0;
    g_wldcore_window_panel_render_state.unknown_18[0] = 0;
    g_wldcore_window_panel_render_state.unknown_18[1] = 0;
    g_wldcore_window_panel_render_state.text_id = 0;
    g_wldcore_window_panel_render_state.window_y = origin_coordinate - 0x78;
    final_height = dimensions->y;
    data_pointer = g_wldcore_panel_row_actions;
    g_wldcore_window_panel_render_state.parent_indices = data_pointer;
    value = 1;
    g_wldcore_window_panel_render_state.field_0x20 = 0;
    g_wldcore_window_panel_render_state.max_row_index = final_height;
    g_wldcore_window_panel_render_state.header_id = value;
    g_wldcore_window_panel_render_state.text_binding = (struct world_menu_text_binding*)label; /* a column layout */
    g_wldcore_window_panel_render_state.value = &g_wldcore_active_menu_value;
}
