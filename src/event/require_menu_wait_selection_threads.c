#include "fft/require.h"
#include "fft/thread.h"
#include "psx/types.h"

void require_menu_wait_selection_threads(void) {
    while (g_battle_threads[5].is_running != 0) {
        s32 selection = g_require_menu_selection_result;

        if (selection == -1) {
            break;
        }
        if (selection == 1) {
            break;
        }
        if (selection == 0) {
            break;
        }
        battle_thread_yield();
    }
    battle_thread_yield();
    g_battle_threads[7].task_id = NATIVE_THREAD_TASK_STOP_REQUEST;
    battle_thread_set_parameters(7, 0, 0, 2);
    battle_thread_wait_until_inactive(7);
    battle_thread_wait_until_inactive(5);
}
