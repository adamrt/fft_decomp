/* The change banner and stat preview panel thread toggles. */

#include "fft/menu.h"
#include "fft/menu_list.h"
#include "fft/menu_overlay.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "fft/world_menu.h"
#include "psx/types.h"

/* Open or close the equipment-change panel and its numeric editor. While the status panel
 * slides, an open request does nothing, so per-frame callers open it once the slide ends. */
void world_menu_toggle_change_banner_panel_thread(s32 enable) {
    s32 state = enable;

    if (state != 0) {
        state = 1;
        if (!(g_menu_status_display_thread_params.flags
                & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP))) {
            if (thread_is_running(WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL) == 0) {
                thread_start(WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL, menu_run_equipment_list_thread);
                thread_set_parameters(
                    WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL, (s32)&g_world_change_banner_panel_thread_params, 0, 0);
            }
            menu_toggle_numeric_editor_thread(state);
        }
    } else {
        menu_request_thread_stop(WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
    }
}

/* Open or close the stat preview panel: menu_run_equipment_list_thread in the character status
 * slot, with g_world_stat_preview_panel_thread_params. Like the change banner, an open request
 * does nothing while the status panel slides. The normalized enable state (0 or 1) is stored in
 * g_world_change_banner_panel_thread_params.style either way. */
void world_menu_toggle_stat_preview_panel_thread(s32 enable) {
    if (enable != 0) {
        if (!(g_menu_status_display_thread_params.flags & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP))
            && thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0) {
            thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, menu_run_equipment_list_thread);
            thread_set_parameters(
                THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_world_stat_preview_panel_thread_params, 0, 0);
        }
        enable = 1;
    } else {
        menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
    }
    g_world_change_banner_panel_thread_params.style = enable;
}
