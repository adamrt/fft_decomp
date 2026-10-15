#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "fft/thread.h"

/* Start the mini menu when its slots are idle and menus are active. */
void menu_start_mini_menu_display_thread(void) {
    if (thread_is_running_asm_variant(THREAD_SLOT_MENU) != 0) {
        return;
    }
    if (thread_is_running_asm_variant(THREAD_SLOT_MENU_CHILD) != 0) {
        return;
    }
    if (g_menu_overlay_state != 0) {
        return;
    }
    g_menu_current_id = BATTLE_MENU_ID_MINI_MENU;
    g_menu_thread_menu_data = g_menu_idle_action_entries;
    thread_start(THREAD_SLOT_MENU, menu_run_triangle_selection);
    thread_set_parameters(THREAD_SLOT_MENU, (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_MINI_MENU], 0, 0);
    g_thread_contexts[THREAD_SLOT_MENU].task_id = THREAD_TASK_MINI_MENU;
}
