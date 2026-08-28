#include "fft/battle.h"
#include "fft/thread.h"

enum {
    MINI_MENU_THREAD_ID = 8,
    MINI_MENU_MENU_ID = 10,
};

void battle_menu_start_mini_menu_display_thread(void) {
    if (battle_thread_is_running_8014cc94(MINI_MENU_THREAD_ID) != 0) {
        return;
    }
    if (battle_thread_is_running_8014cc94(7) != 0) {
        return;
    }
    if (g_option_menu_submenu_state != 0) {
        return;
    }
    g_battle_menu_current_id = MINI_MENU_MENU_ID;
    g_battle_menu_thread_menu_data = g_battle_menu_idle_action_entries;
    battle_thread_start(MINI_MENU_THREAD_ID, battle_menu_display_triangle_selection);
    battle_thread_set_parameters(MINI_MENU_THREAD_ID, (s32)&g_battle_menu_thread_menu_data[10], 0, 0);
    g_battle_thread_contexts[8].task_id = NATIVE_THREAD_TASK_MENU_SOUND_DELAY;
}
