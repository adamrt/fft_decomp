#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

void wldcore_bar_push_rumor_detail_level(s32 value) {
    s32 coordinate_mode = 0;
    s32 depth;
    u8 unused[8];

    g_text_substitution_values[0] = value + TEXT_ID_RUMOR_NAME_BASE;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_RUMOR_ABOUT, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].rumor_detail.rumor_index = value;
    g_wldcore_scrollable_text_window.text_id = value + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 1:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x70;
        break;
    case 0:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x60;
        break;
    }
    g_wldcore_scrollable_text_window.base.x = -122;
    g_wldcore_scrollable_text_window.base.y = -40;
    g_wldcore_scrollable_text_window.text_width = 0xDC;
    g_wldcore_scrollable_text_window.rows_per_page = 8;
    g_wldcore_scrollable_text_window.priority = 9;
    g_wldcore_scrollable_text_window.image_coordinate_mode = coordinate_mode;
    g_wldcore_scrollable_text_window.extra_render_index = -1;
    g_wldcore_scrollable_text_window.text_substitutions[1] = -1;
    g_wldcore_scrollable_text_window.text_substitutions[0] = -1;
    wldcore_text_init_scrollable_window(&g_wldcore_scrollable_text_window);
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.first_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_records_next[depth].rumor_detail.first_window
        = g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.first_window;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.second_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_records_next[depth].rumor_detail.second_window
        = g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.second_window;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.render_index].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_menu_stack_records_next[depth].rumor_detail.render_index
        = g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.render_index;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_RUMOR_DETAIL;
}
