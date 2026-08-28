#include "fft/battle.h"
#include "psx/types.h"

/* Twin of battle_menu_request_open_companion_executable at 0x8013bc14: same wait
 * protocol, but the request is published through g_battle_menu_secondary_companion_executable_request_index.
 * Request 2 opens EVENT/WIN001.BIN, 3 EVENT/BONUS.BIN and 4 EVENT/SPELL.MES: the
 * LBA and size tables at 0x80168fb8/0x80168ff8 name those files. */
void battle_menu_request_open_secondary_companion_executable(s32 request_index) {
    while (g_battle_menu_companion_executable_request_index != 0
        || g_battle_menu_secondary_companion_executable_request_index != 0) {
        battle_thread_yield();
    }
    g_battle_menu_secondary_companion_executable_request_index = request_index;
    if (request_index != 0) {
        do {
            battle_thread_yield();
        } while (g_battle_menu_secondary_companion_executable_request_index != 0);
    }
    if (g_battle_script_frame_result_override == 1) {
        battle_thread_exit_current();
    }
}
