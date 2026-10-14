#include "fft/battle.h"
#include "fft/thread.h"

/* Thread entry of the turn menus (entries 0x00, 0x0e, 0x13, 0x14, 0x20, 0x21 and
 * 0x2e-0x33 of the idle action menu table): run the icon selection loop, then
 * remember the chosen row for the active-turn unit and exit. */
void battle_menu_build_main(void) {
    menu_entry_t* parameter;
    menu_unit_selection_record_t* record;
    s32 unit_id;

    parameter = (menu_entry_t*)battle_thread_get_current_parameter_1();
    unit_id = g_menu_selected_unit.battle_id;
    record = &g_menu_unit_selection_records[unit_id];
    /* A nonzero second parameter makes the icon loop return here instead of exiting. */
    g_thread_contexts[g_thread_current_id].function_parameter_2 = 1;
    battle_menu_run_icon_selection_loop();
    record->command_index = parameter->selected_index;
    thread_exit_current();
}
