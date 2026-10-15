/* High overlay reservation and companion executable/data requests. */

#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Yield until the allocation reaches the high overlay address. */
void menu_reserve_high_overlay(u32 size) {
    while (1) {
        if (main_heap_reserve_at(g_main_heap_high_overlay_load_address, size)
            == g_main_heap_high_overlay_load_address) {
            break;
        }
        thread_yield();
        if (g_main_font_print_enabled != 0) {
            FntPrint(g_menu_wait_for_file_allocation_message);
        }
    }
}

void menu_free_high_overlay(void) {
    main_heap_free(g_main_heap_high_overlay_load_address);
}

/* Publish an executable request, wait for it to clear, then honor the frame exit request. */
void menu_request_open_companion_executable(s32 request_index) {
    while (g_menu_companion_executable_request_index != 0 || g_menu_companion_data_file_request_index != 0) {
        thread_yield();
    }
    g_menu_companion_executable_request_index = request_index;
    if (request_index != 0) {
        do {
            thread_yield();
        } while (g_menu_companion_executable_request_index != 0);
    }
    if (g_script_frame_result_override == 1) {
        thread_exit_current();
    }
}

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
