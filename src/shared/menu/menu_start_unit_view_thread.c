#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Requests a unit view mode from the unit view supervisor thread, starting the
 * thread when it is not running.
 *
 * The unit name list is rebuilt and the panel fade mode taken from
 * g_battle_menu_panel_fade_modes[mode]. unit_a becomes the active-turn unit of
 * the banner and unit_b the previewed target of the status billboard; 0xff leaves
 * either unchanged. Ignored while g_menu_overlay_state is 1. */
void menu_start_unit_view_thread(s32 mode, s32 unit_a, s32 unit_b) {
    if (g_menu_overlay_state == 1) {
        return;
    }
    text_build_unit_name_list();
    g_battle_menu_panel_fade_mode = g_battle_menu_panel_fade_modes[mode];
    if (unit_a != 0xff) {
        g_menu_selected_unit.battle_id = unit_a;
        g_menu_selected_unit_status.unit_index = unit_a;
    }
    if (unit_b != 0xff) {
        g_menu_comparison_battle_id = unit_b;
        g_menu_comparison_unit_status.unit_index = unit_b;
    }
    if (battle_thread_is_running_asm(THREAD_SLOT_UNIT_VIEW) == 0) {
        thread_start(THREAD_SLOT_UNIT_VIEW, menu_run_unit_view_supervisor_thread);
    }
    thread_set_parameters(THREAD_SLOT_UNIT_VIEW, mode, 0, 0);
}
