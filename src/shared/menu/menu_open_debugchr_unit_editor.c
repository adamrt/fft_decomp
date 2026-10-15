#include "fft/battle_menu.h"
#include "fft/event_debugchr.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"

/* Open the unit editor for a rider or mount, then restore its menu view. */
void menu_open_debugchr_unit_editor(void) {
    s16* unit_index = &g_menu_selected_unit.battle_id;

    if (battle_unit_get_stats_from_battle_id(*unit_index)->mount_info != 0) {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
        menu_set_modal_open();
        g_menu_overlay_state = 1;
        menu_request_open_companion_executable(COMPANION_OVERLAY_DEBUGCHR);
        thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
        thread_wait_until_inactive(THREAD_SLOT_MENU);
        thread_wait_for_unit_view_panels();
        menu_record_selection_for_selected_unit();
        battle_menu_enter_status_screen_selection();
        debugchr_editor_run_unit_thread(*unit_index);
        g_menu_overlay_state = 0;
        g_sound_suppressed = 0;
        g_script_event_input = 0;
        menu_build_idle_action_menu_variant(g_menu_current_id);
        menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, *unit_index, *unit_index);
        menu_clear_modal_open();
        battle_menu_leave_status_screen_selection();
        thread_exit_current();
    }
}
