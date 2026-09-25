#include "fft/battle.h"
#include "fft/event_helpmenu.h"
#include "psx/types.h"

void battle_menu_building_thread(void) {
    if (battle_thread_find_running_by_task(NATIVE_THREAD_TASK_UNIT_EDITOR_PANEL) != 0
        && battle_thread_find_running_by_task(NATIVE_THREAD_TASK_MENU_WINDOW_BUILD) == 0) {
        battle_sound_set_effect_to_confirm();
    }
    battle_menu_retry_alloc_with_message(0x20000);
    battle_menu_request_open_companion_executable(3);
    helpmenu_run_battle_help_menu();
}
