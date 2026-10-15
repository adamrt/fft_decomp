#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"

/*
 * Soldier office: confirm hiring the previewed recruit.
 *
 * Selection 1 or -1 declines and removes the recruit from the roster;
 * selection 2 returns to the preview. Any other result shows a message and
 * waits for its thread before going on to the name entry.
 */
void world_shop_run_hire_confirm_step(void) {
    s16 selection;
    if (g_world_shop_hire_confirm_message_wait != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            g_world_shop_hire_confirm_message_wait = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_NAME_ENTRY;
        }
    } else if (menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, &g_world_shop_hire_confirm_menu) == 0) {
        selection = g_world_menu_selection_results[2];
        if (selection == 1 || selection == -1) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF827, 0);
            g_world_shop_hire_menu_initialized = 0;
            main_party_remove_unit(g_world_unit_pointers[g_unit_selected_index]->identity.roster_slot);
            g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_MENU;
        } else if (selection == 2) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
            g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_PREVIEW;
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF829, 0);
            g_world_shop_hire_confirm_message_wait = 1;
        }
    }
    world_shop_run_obtain_gil_menu();
    world_shop_run_soldier_office_fee_menu_script();
    world_menu_run_script_with_callback_suppressed();
}
