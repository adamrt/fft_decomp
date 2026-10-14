#include "fft/battle_menu.h"
#include "fft/menu.h"

/* Restore the selected unit's remembered command row, applying menu remaps. */
void menu_restore_selection_for_selected_unit(s32 menu_id) {
    menu_unit_selection_record_t* record;
    s32 i;
    s32 row;
    s32 candidate;
    menu_selection_remap_t* remap;

    for (i = 0; i < 12; i++) {
        if (g_menu_command_maps[i].menu_id == menu_id) {
            break;
        }
    }
    if (i == 12) {
        return;
    }
    record = &g_menu_unit_selection_records[g_menu_selected_unit.battle_id];
    for (row = 0, remap = g_menu_selection_remaps; row < 8; remap++, row++) {
        if (menu_id == g_menu_selection_remaps[row].menu_id) {
            /* Keep the signed byte-table index used by the target. */
            for (candidate = 2; candidate < 5; candidate++) {
                if (record->command_index == remap->matches[candidate - 2]) {
                    record->command_index = g_menu_selection_remaps[row].replacement;
                    break;
                }
            }
        }
    }
    g_menu_thread_menu_data[menu_id].selected_index = record->command_index;
}
