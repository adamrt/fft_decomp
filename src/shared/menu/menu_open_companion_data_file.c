#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/menu.h"
#include "fft/thread.h"

/* Non-blocking two-step file load driven by g_thread_call_target:
 * issue the read for the pending request, then poll until it completes and
 * clear the request. */
void menu_open_companion_data_file(void) {
    if ((g_menu_companion_data_file_request_index != 0) && (g_menu_companion_data_file_loading == 0)) {
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
        if (thread_call_on_main_stack(g_menu_companion_data_file_lbas[g_menu_companion_data_file_request_index],
                g_menu_companion_data_file_sizes[g_menu_companion_data_file_request_index],
                g_menu_companion_data_file_buffer)
            == 0) {
            g_menu_companion_data_file_loading = 1;
        }
    } else if ((g_menu_companion_data_file_request_index != 0) && (g_menu_companion_data_file_loading != 0)) {
        g_thread_call_target = main_file_is_still_loading;
        if (thread_call_on_main_stack() == 0) {
            g_menu_companion_data_file_loading = 0;
            g_menu_companion_data_file_request_index = 0;
        }
    }
}
