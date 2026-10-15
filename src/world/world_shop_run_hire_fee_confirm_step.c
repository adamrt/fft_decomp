#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"

/*
 * Soldier office: confirm the hiring fee.
 *
 * Choice 0 pays the fee and stores the recruit's name; 1 or -1 removes the
 * recruit from the roster. Other choices return to the name entry.
 */
void world_shop_run_hire_fee_confirm_step(void) {
    s16 choice;
    g_menu_help_message_text_id = 0;
    if (g_world_shop_hire_fee_message_wait != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_REOPEN;
            g_world_shop_hire_fee_message_wait = 0;
        }
    } else if (menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, &g_world_shop_hire_fee_confirm_menu) == 0) {
        choice = g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT];
        if (choice == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf82b, 0);
            world_shop_adjust_gil(-g_world_shop_service_fee);
            world_formation_set_unit_name(g_unit_selected_index, g_world_shop_entered_unit_name);
            g_world_shop_hire_fee_message_wait = 1;
        } else if (choice == 1 || choice == -1) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf827, 0);
            g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_MENU;
            main_party_remove_unit(g_world_unit_pointers[g_unit_selected_index]->identity.roster_slot);
            g_world_shop_hire_menu_initialized = 0;
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
            g_world_shop_hire_fee_confirm_menu.selected_index = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_NAME_ENTRY;
        }
    }
    world_shop_run_obtain_gil_menu();
    world_shop_run_soldier_office_fee_menu_script();
    world_menu_run_script_with_callback_suppressed();
}
