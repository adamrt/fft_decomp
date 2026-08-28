#include "fft/world.h"
#include "psx/types.h"

void world_formation_stop_menu_threads(void) {
    world_menu_stop_unit_status_banner_thread(8);
    world_menu_stop_unit_status_banner_thread(7);
    world_menu_stop_unit_status_banner_thread(0xA);
    world_menu_stop_unit_status_banner_thread(0xC);
    world_menu_toggle_preview_stats_window(0);
    world_menu_toggle_stat_preview_panel_thread(0);
    g_world_preview_stats_thread_params.style = 0;
    g_world_stat_preview_panel_thread_params.style = 0;
    g_world_equipment_ability_panel_thread_params.style = 0;
    g_world_ability_panel_thread_params.style = 0;
}
