#include "fft/wldcore.h"

/* Opens a full-screen menu level (type 5): hides the map, loads data set
 * 0x16 or 0x17 by `EVENT_SCRIPT_VAR_TOWN_BACKGROUND`, starts WORLD thread 14 on
 * 0x8010f250 with text 0xb806, and allocates two window records and one
 * render record for the level before 0x8007148c fills it. */
void wldcore_bar_push_menu_level(void) {
    s32 value;
    s32 set;
    s32 record_index;

    wldcore_clear_screen_and_set_map_clip_rect(1);
    wldcore_window_set_render_objects_visible(0);
    wldcore_fade_start_screen(0, 0x10);
    value = world_script_get_variable(EVENT_SCRIPT_VAR_TOWN_BACKGROUND);
    set = value == 1 ? 0x16 : 0;
    if (value == 2) {
        set = 0x17;
    }
    wldcore_load_data_set_into_scratch_buffer(set);
    wldcore_wait_for_file_load();
    g_main_system_flags |= 0x3000;
    world_thread_start(0xE, world_text_message_box_thread);
    world_thread_set_parameters(0xE, 0x19, 0xB806, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_pair_render.first_window
        = wldcore_window_append_record_and_reset_color(
            g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_pair_render.second_window
        = wldcore_window_append_record_and_reset_color(
            g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_pair_render.render_index
        = wldcore_window_append_render_record_and_reset_color(
            g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    wldcore_bar_build_menu_rows(&g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[record_index].window_pair_render._unknown_0c = 0;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_BAR_MENU;
    g_wldcore_menu_stack_depth = record_index + 1;
}
