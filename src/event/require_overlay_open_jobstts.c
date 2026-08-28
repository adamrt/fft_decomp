#include "fft/battle.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/require.h"
#include "psx/types.h"

void require_overlay_open_jobstts(void) {
    void* allocation;

    battle_action_copy_active_turn_data_from(
        g_require_panel_active_unit_banner, g_require_active_unit_data, g_require_panel_billboard_data);
    while (1) {
        allocation = main_heap_reserve_at(g_main_heap_high_overlay_load_address, 0x1f000);
        if (allocation == g_main_heap_high_overlay_load_address) {
            break;
        }
        battle_thread_yield();
        if (g_font_print_enabled != 0) {
            FntPrint(g_jobstts_text_wait_for_allocation_message);
        }
    }
    battle_menu_request_open_companion_executable(7);
    battle_thread_wait_frames(2);
    g_companion_overlay_state = 3;
    do {
        g_require_input_controller = battle_script_get_controller_input_pointer(0);
        *g_require_input_controller = 0;
        battle_thread_yield();
    } while (g_companion_overlay_state != 0);
    *g_require_input_controller = 0;
    main_heap_free(allocation);
    battle_thread_wait_frames(2);
    g_option_menu_open = 0;
    battle_thread_exit_current();
}
