#include "fft/wldcore.h"

/* Pushes the number-panel menu level (type 0x30) that edits script variable
 * `variable_id`, starting the panel at `value`. */
void wldcore_menu_push_number_panel_level_with_argument(void* variable_id, s16 value) {
    wldcore_point32_t origin;
    s32 record_index;

    g_wldcore_menu_result = 0x60;
    g_wldcore_menu_ordering_table_offset = 2;
    g_wldcore_active_menu_value = value;
    origin.x = 0x20;
    origin.y = 0x20;
    wldcore_window_init_number_panel_render_thread(0xC, 0x270F0000, 0, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[record_index].panel.result = 0;
    g_wldcore_menu_stack_records_next[record_index].panel.argument = variable_id;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_NUMBER_PANEL_WITH_ARGUMENT;
    g_wldcore_menu_stack_depth = record_index + 1;
}
