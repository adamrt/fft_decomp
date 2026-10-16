/* The change banner and stat preview panel thread toggles. */

#include "fft/formation.h"
#include "fft/menu.h"
#include "fft/menu_list.h"
#include "fft/screen.h"
#include "fft/thread.h"
#include "fft/unit_panel.h"
#include "psx/types.h"

/* Open or close the equipment-change panel and its numeric stats panel. While the status panel
 * slides, an open request does nothing, so per-frame callers open it once the slide ends. */
void formation_toggle_change_panel(s32 enable) {
    s32 state = enable;

    if (state != 0) {
        state = 1;
        if (!(g_unit_panel_status_display_thread_params.flags
                & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP))) {
            if (thread_is_running(SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL) == 0) {
                thread_start(SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL, unit_panel_equipment_thread);
                thread_set_parameters(
                    SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL, (s32)&g_formation_change_banner_thread_params, 0, 0);
            }
            unit_panel_toggle_numbers_thread(state);
        }
    } else {
        menu_request_thread_stop(SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
    }
}

/* Open or close the stat preview panel: unit_panel_equipment_thread in the character status
 * slot, with g_formation_stat_preview_panel_thread_params. Like the change banner, an open request
 * does nothing while the status panel slides. The normalized enable state (0 or 1) is stored in
 * g_formation_change_banner_thread_params.style either way. */
void formation_toggle_stat_preview_panel(s32 enable) {
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
        menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
    }
    g_formation_change_banner_thread_params.style = enable;
}
