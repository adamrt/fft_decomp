/* Equipment/ability panel thread start, redraw requests and formation thread stops. */

#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_menu_start_equipment_and_ability_panel_thread(s32 enabled) {
    if (enabled == 0) {
        return;
    }
    enabled = 1;
    if (g_menu_status_display_thread_params.flags & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP)) {
        return;
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0
        && thread_is_running(WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL) == 0) {
        thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, menu_run_equipment_ability_panel_thread);
        thread_set_parameters(
            THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_world_equipment_ability_panel_thread_params, 0, 0);
    }
    menu_toggle_numeric_editor_thread(enabled);
}

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
