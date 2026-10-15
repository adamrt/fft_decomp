#include "fft/world.h"

/* Request a redraw from all eight formation panel threads. */
void world_menu_enable_all_order_entries(void) {
    g_menu_status_display_thread_params.redraw_request = 1;
    g_menu_comparison_display_thread_params.redraw_request = 1;
    g_world_equipment_ability_panel_thread_params.redraw_request = 1;
    g_menu_numeric_panel_params.redraw_request = 1;
    g_menu_preview_stats_thread_params.redraw_request = 1;
    g_world_change_banner_panel_thread_params.redraw_request = 1;
    g_world_stat_preview_panel_thread_params.redraw_request = 1;
    g_world_ability_panel_thread_params.redraw_request = 1;
}
