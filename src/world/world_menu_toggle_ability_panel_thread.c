#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Starts thread 10 on world_menu_ability_panel_thread (parameter g_world_ability_panel_thread_params) when `enable` is
 * set and neither thread 9 nor 10 is running; stops threads 10 and 12 otherwise. */
void world_menu_toggle_ability_panel_thread(s32 enable) {

    if (enable != 0) {
        enable = 1;
        if (thread_is_running(WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL) == 0
            && thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) == 0) {
            thread_start(WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL, menu_run_ability_list_thread_variant);
            thread_set_parameters(
                WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL, (s32)&g_world_ability_panel_thread_params, 0, 0);
        }
        g_world_formation_unit_banner_enabled = 1;
        menu_toggle_numeric_editor_thread(enable);
    } else {
        menu_request_thread_stop(WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
    }
}
