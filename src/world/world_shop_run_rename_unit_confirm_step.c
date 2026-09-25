#include "fft/world.h"
#include "psx/pad.h"

/*
 * Process the selected unit's quoted-cost and name confirmation.
 *
 * Choice 0 pays the quoted cost and stores the name; 1 or -1 removes the
 * selected roster slot. Other choices return to the name-entry state.
 */
void world_shop_run_rename_unit_confirm_step(void) {
    s16 choice;
    g_world_menu_description_text_id = 0;
    if (g_world_shop_rename_message_wait != 0) {
        if (g_world_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_world_shop_menu_step = 0x19;
            g_world_shop_rename_message_wait = 0;
        }
    } else if (world_menu_run_thread(15, &g_world_shop_rename_confirm_menu) == 0) {
        choice = g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT];
        if (choice == 0) {
            world_thread_set_parameters(2, 0x19, 0xf82b, 0);
            world_shop_obtain_gil(-g_world_shop_service_fee);
            world_formation_set_unit_name(g_world_formation_selected_unit_index, g_world_shop_entered_unit_name);
            g_world_shop_rename_message_wait = 1;
        } else if (choice == 1 || choice == -1) {
            world_thread_set_parameters(2, 0x19, 0xf827, 0);
            g_world_shop_menu_step = 0x14;
            main_party_remove_unit(g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->roster_slot);
            g_world_shop_hire_menu_initialized = 0;
        } else {
            world_thread_set_parameters(2, 0x19, -1, 0);
            g_world_shop_rename_confirm_menu.cursor = 0;
            g_world_shop_menu_step = 0x17;
        }
    }
    world_shop_run_obtain_gil_menu();
    world_shop_run_soldier_office_fee_menu_script();
    world_menu_run_script_with_callback_suppressed();
}
