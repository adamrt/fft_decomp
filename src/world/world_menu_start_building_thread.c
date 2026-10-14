#include "fft/battle_camera.h"
#include "fft/thread.h"
#include "fft/world.h"

/* Start the help-menu thread in the menu-building slot when nothing prevents
 * it: no thread-change cooldown, menu transition, active camera move or
 * companion overlay, and the slot idle. A menu other than the mini menu that
 * is already open or building is flagged first. */
void world_menu_start_building_thread(void) {
    if (world_thread_is_running_asm(THREAD_SLOT_MENU) || world_thread_is_running_asm(THREAD_SLOT_MENU_BUILDING)) {
        if (g_menu_current_id != BATTLE_MENU_ID_MINI_MENU) {
            g_world_menu_building_thread_started = 1;
        }
    }
    if (g_world_thread_change_cooldown == 0 && g_menu_modal_open == 0 && camera_is_active() == 0
        && g_world_companion_overlay_state == COMPANION_OVERLAY_STATE_IDLE
        && world_thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) == 0) {
        g_world_text_overlay_active = 1;
        thread_start(THREAD_SLOT_MENU_BUILDING, world_menu_run_helpmenu_thread);
        thread_set_parameters(THREAD_SLOT_MENU_BUILDING, 0, 0, 0);
    }
}
