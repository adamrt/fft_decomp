#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void battle_menu_open_companion_executable(void) {
    s32 idx;

    idx = g_battle_menu_companion_executable_request_index;
    if ((idx != 0) && (g_battle_menu_companion_executable_loading == 0)) {
        g_battle_thread_call_target = (void (*)(void))main_file_call_build_header;
        if (battle_thread_call_on_main_stack(g_battle_menu_companion_executable_lbas[idx],
                g_battle_menu_companion_executable_sizes[idx],
                g_battle_menu_companion_executable_offsets[idx] + (s32)g_event_overlay_load_address)
            == 0) {
            g_battle_menu_companion_executable_loading = 1;
        }
    } else if ((g_battle_menu_companion_executable_request_index != 0)
        && (g_battle_menu_companion_executable_loading != 0)) {
        g_battle_thread_call_target = main_file_is_still_loading;
        if (battle_thread_call_on_main_stack() == 0) {
            g_battle_menu_companion_executable_loading = 0;
            g_battle_menu_companion_executable_request_index = 0;
        }
    }
}
