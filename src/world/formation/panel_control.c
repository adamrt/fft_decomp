/* Thread toggles for the formation screen's equipment panels: the selected unit's equipment panel
 * with its numeric stats panel, and the preview equipment panel that lists a previewed loadout. */

#include "fft/menu/menu_list.h"
#include "fft/menu/menu_screen.h"
#include "fft/party/formation.h"
#include "fft/party/unit_panel.h"
#include "fft/sys/thread.h"
#include "psx/types.h"

/* Open or close the equipment-change panel and its numeric stats panel. While the status panel
 * slides, an open request does nothing, so per-frame callers open it once the slide ends. */
void unit_panel_toggle_equipment_threads(s32 enable) {
    if (enable != 0) {
        enable = 1;
        if (!(g_unit_panel_status_display_thread_params.flags
                & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP))) {
            if (thread_is_running(MENU_SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL) == 0) {
                thread_start(MENU_SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL, unit_panel_equipment_thread);
                thread_set_parameters(
                    MENU_SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL, (s32)&g_formation_change_banner_thread_params, 0, 0);
            }
            unit_panel_toggle_numbers_thread(enable);
        }
    } else {
        menu_thread_request_stop(MENU_SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL);
        menu_thread_request_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
    }
}

/* Open or close the preview equipment panel: unit_panel_equipment_thread in the character status
 * slot, with g_formation_stat_preview_panel_thread_params, where it lists the five items of
 * g_item_preview_stat_detail (the best-fit or shop loadout being previewed). Like the change banner,
 * an open request does nothing while the status panel slides. The normalized enable state (0 or 1) is stored in
 * g_formation_change_banner_thread_params.style either way. */
void formation_toggle_preview_equipment_panel(s32 enable) {
    if (enable != 0) {
        if (!(g_unit_panel_status_display_thread_params.flags
                & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP))
            && thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0) {
            thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, unit_panel_equipment_thread);
            thread_set_parameters(
                THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_formation_stat_preview_panel_thread_params, 0, 0);
        }
        enable = 1;
    } else {
        menu_thread_request_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
    }
    g_formation_change_banner_thread_params.style = enable;
}
