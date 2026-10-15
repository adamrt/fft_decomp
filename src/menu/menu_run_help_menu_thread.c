#include "fft/event_helpmenu.h"
#include "fft/menu.h"
#include "fft/sound.h"
#include "fft/thread.h"

/* Reserve and load the help overlay, then run its battle-help screen. */
void menu_run_help_menu_thread(void) {
    if (thread_find_running_by_task(THREAD_TASK_UNIT_EDITOR_PANEL) != 0
        && thread_find_running_by_task(THREAD_TASK_MENU_WINDOW_BUILD) == 0) {
        sound_queue_sfx_confirm();
    }
    menu_reserve_high_overlay(0x20000);
    menu_request_open_companion_executable(COMPANION_OVERLAY_HELPMENU);
    menu_run_battle_help();
}
