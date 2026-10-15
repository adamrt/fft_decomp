/* Change banner and stat preview panel toggles. */

#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Starts thread 10 on world_menu_equipment_panel_thread (parameter g_world_change_banner_panel_thread_params) when
 * `enable` is set and neither of the g_world_status_display_thread_params.flags 0x60 bits is set; stops threads 10 and
 * 12 otherwise.
 */
void menu_toggle_change_banner_panel_thread(s32 enable) {
    s32 state = enable;

    if (state != 0) {
        state = 1;
        if (!(g_menu_status_display_thread_params.flags
                & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP))) {
            if (thread_is_running(WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL) == 0) {
                thread_start(WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL, menu_run_equipment_list_thread_variant);
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

/* Starts thread 9 on world_menu_equipment_panel_thread (parameter g_world_stat_preview_panel_thread_params) when
 * `enable` is set and neither of the g_world_status_display_thread_params.flags 0x60 bits is set; stops it otherwise.
 * g_world_change_banner_panel_thread_params.style records the normalized enable state. */
void menu_toggle_stat_preview_panel_thread(s32 enable) {
    if (enable != 0) {
        if (!(g_menu_status_display_thread_params.flags & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP))
            && thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0) {
            thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, menu_run_equipment_list_thread_variant);
            thread_set_parameters(
                THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_world_stat_preview_panel_thread_params, 0, 0);
        }
        enable = 1;
    } else {
        menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
    }
    g_world_change_banner_panel_thread_params.style = enable;
}
