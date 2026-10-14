#include "fft/battle_action.h"
#include "fft/menu.h"

/* Record the current menu's selection byte for the selected unit, when that
 * menu appears in the tracked menu-id table. */
void menu_record_selection_for_selected_unit(void) {
    menu_unit_selection_record_t* records;
    s16* selected;
    s32 i;

    selected = &g_menu_selected_unit.battle_id;
    records = g_menu_unit_selection_records;
    for (i = 0; i < 12; i++) {
        if (g_menu_current_id == g_menu_command_maps[i].menu_id) {
            records[*selected].command_index = g_menu_thread_menu_data[g_menu_current_id].selected_index;
        }
    }
}
