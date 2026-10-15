#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/event_require.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"

/*
 * Thread body that runs the JOBSTTS overlay from the REQUIRE screen.
 *
 * Copies the selected unit's billboard in, waits until 0x1f000 bytes can be
 * reserved at the high overlay address, requests the overlay and hands the
 * frame loop the JOBSTTS state. Until that state returns to idle the
 * 32-bit controller value is zeroed every frame so this screen sees no input. Then
 * the reservation is freed and the option-menu flag cleared.
 */
void require_overlay_open_jobstts(void) {
    void* allocation;

    menu_copy_billboard_data_in(&g_menu_selected_unit_status, &g_menu_selected_unit, &g_menu_selected_unit_fields);
    while (1) {
        allocation = main_heap_reserve_at(g_main_heap_high_overlay_load_address, 0x1f000);
        if (allocation == g_main_heap_high_overlay_load_address) {
            break;
        }
        thread_yield();
        if (g_main_font_print_enabled != 0) {
            FntPrint(g_require_text_wait_for_file_allocation_message);
        }
    }
    menu_request_open_companion_executable(COMPANION_OVERLAY_JOBSTTS);
    thread_wait_frames(2);
    g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_JOBSTTS;
    do {
        g_menu_input_controller = script_get_controller_input_pointer(0);
        *g_menu_input_controller = 0;
        thread_yield();
    } while (g_menu_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);
    *g_menu_input_controller = 0;
    main_heap_free(allocation);
    thread_wait_frames(2);
    g_menu_modal_open = 0;
    thread_exit_current();
}
