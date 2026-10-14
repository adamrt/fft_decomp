#include "fft/thread.h"
#include "fft/world.h"

/* Menu thread of the main command menu: requests a redraw, runs the icon
 * selection loop and stores the chosen row in byte 0 of the active unit's
 * menu record. */
void world_menu_build_main(void) {
    menu_entry_t* entry;
    menu_unit_selection_record_t* row;

    entry = (menu_entry_t*)world_thread_get_current_parameter_1();
    row = &g_menu_unit_selection_records[g_menu_selected_unit.battle_id];
    g_thread_contexts[g_thread_current_id].function_parameter_2 = 1; /* redraw request */
    world_menu_run_icon_selection_loop();
    row->command_index = entry->selected_index;
    thread_exit_current();
}
