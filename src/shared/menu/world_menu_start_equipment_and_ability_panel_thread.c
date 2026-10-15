#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Start thread 9 on menu_run_equipment_ability_panel_thread unless threads 9/10 already run. */
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
