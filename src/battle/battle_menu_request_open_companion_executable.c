#include "fft/battle.h"
#include "psx/types.h"

/* Waits until neither companion executable request is pending, publishes
 * request_index and, when it is non-zero, yields until it is cleared. The
 * calling thread then exits when g_battle_script_frame_result_override is 1. */
void battle_menu_request_open_companion_executable(s32 request_index) {
    while (g_battle_menu_companion_executable_request_index != 0
        || g_battle_menu_secondary_companion_executable_request_index != 0) {
        battle_thread_yield();
    }
    g_battle_menu_companion_executable_request_index = request_index;
    if (request_index != 0) {
        do {
            battle_thread_yield();
        } while (g_battle_menu_companion_executable_request_index != 0);
    }
    if (g_battle_script_frame_result_override == 1) {
        battle_thread_exit_current();
    }
}
