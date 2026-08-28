#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

void wldcore_menu_push_number_panel_level(void) {
    wldcore_point32_t origin;
    s32 record_index;
    s32 x;
    s32 y;

    x = g_wldcore_selected_proposition_row[0].fields.min_days;
    y = g_wldcore_selected_proposition_row[0].fields.max_days;
    g_world_text_substitution_values[0] = x;
    g_world_text_substitution_values[1] = y;
    world_thread_set_parameters(0xE, 0x19, 0xB813, 0);
    g_wldcore_menu_result = 0x60;
    g_wldcore_menu_ordering_table_offset = 8;
    origin.x = 0x38;
    origin.y = 0x20;
    g_wldcore_active_menu_value = x;
    wldcore_window_init_number_panel_render_thread(0xC, x | (y << 16), 0xB823, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[record_index].panel.result = 0;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_NUMBER_PANEL;
    g_wldcore_menu_stack_depth = record_index + 1;
}
