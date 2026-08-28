#include "fft/data.h"
#include "fft/main_unit.h"
#include "fft/world.h"
#include "psx/pad.h"

/*
 * Process the selected unit's dismissal confirmation and response text.
 *
 * Selection 1 or -1 removes the roster slot; selection 2 cancels. Other
 * results open response text and wait for its thread before advancing.
 */
void world_formation_run_dismiss_unit_confirm_step(void) {
    s16 selection;
    if (g_world_formation_dismiss_message_wait != 0) {
        if (g_world_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            world_thread_set_parameters(2, 0x19, -1, 0);
        }
        if (world_thread_is_running(2) == 0) {
            g_world_formation_dismiss_message_wait = 0;
            g_world_shop_menu_step = 0x17;
        }
    } else if (world_menu_run_thread(0xF, &g_world_formation_dismiss_final_confirm_menu) == 0) {
        selection = g_world_menu_selection_results[2];
        if (selection == 1 || selection == -1) {
            world_thread_set_parameters(2, 0x19, 0xF827, 0);
            g_world_shop_hire_menu_initialized = 0;
            main_party_remove_unit(g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->roster_slot);
            g_world_shop_menu_step = 0x14;
        } else if (selection == 2) {
            world_thread_set_parameters(2, 0x19, -1, 0);
            g_world_shop_menu_step = 0x15;
        } else {
            world_thread_set_parameters(2, 0x19, 0xF829, 0);
            g_world_formation_dismiss_message_wait = 1;
        }
    }
    world_shop_run_obtain_gil_menu();
    world_shop_run_soldier_office_fee_menu_script();
    world_menu_run_script_with_callback_suppressed();
}
