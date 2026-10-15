#include "fft/world.h"

/* Ask the formation screen's panel threads to stop (both status banners, the
 * equipment/ability panel, the numeric editor and the two preview windows)
 * and clear the panel styles. */
void world_formation_stop_menu_threads(void) {
    menu_request_thread_stop(THREAD_SCREEN_SLOT_UNIT_STATUS);
    menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
    menu_request_thread_stop(WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL);
    menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
    world_menu_toggle_preview_stats_window(0);
    menu_toggle_stat_preview_panel_thread(0);
    g_menu_preview_stats_thread_params.style = 0;
    g_world_stat_preview_panel_thread_params.style = 0;
    g_world_equipment_ability_panel_thread_params.style = 0;
    g_world_ability_panel_thread_params.style = 0;
}
