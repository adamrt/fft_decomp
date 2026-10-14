#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Per-frame driver of menu_request_open_companion_executable: start the
 * disc read of the requested overlay on the main stack, at its offset from the
 * companion overlay load address, then poll until the read finishes and clear the
 * request. */
void menu_open_companion_executable(void) {
    s32 idx;

    idx = g_menu_companion_executable_request_index;
    if ((idx != 0) && (g_menu_companion_executable_loading == 0)) {
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
        if (thread_call_on_main_stack(g_menu_companion_executable_lbas[idx], g_menu_companion_executable_sizes[idx],
                g_menu_companion_executable_offsets[idx] + (s32)g_main_heap_companion_overlay_load_address)
            == 0) {
            g_menu_companion_executable_loading = 1;
        }
    } else if ((g_menu_companion_executable_request_index != 0) && (g_menu_companion_executable_loading != 0)) {
        g_thread_call_target = main_file_is_still_loading;
        if (thread_call_on_main_stack() == 0) {
            g_menu_companion_executable_loading = 0;
            g_menu_companion_executable_request_index = 0;
        }
    }
}
