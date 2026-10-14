#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Publish a data-file request, wait for it to clear, then honor the frame exit request. */
void menu_request_open_companion_data_file(s32 request_index) {
    while (g_menu_companion_executable_request_index != 0 || g_menu_companion_data_file_request_index != 0) {
        thread_yield();
    }
    g_menu_companion_data_file_request_index = request_index;
    if (request_index != 0) {
        do {
            thread_yield();
        } while (g_menu_companion_data_file_request_index != 0);
    }
    if (g_script_frame_result_override == 1) {
        thread_exit_current();
    }
}
