#include "fft/battle.h"

/* Record the current menu's selection byte for the selected unit, when that
 * menu appears in the tracked menu-id table. */
void battle_menu_record_selection_for_selected_unit(void) {
    battle_menu_record_t* records;
    s16* selected;
    s32 i;

    selected = &g_battle_active_turn_unit.battle_id;
    records = g_battle_menu_unit_selection_records;
    for (i = 0; i < 12; i++) {
        if (g_battle_menu_current_id == g_battle_menu_id_records[i].menu_id) {
            records[*selected].bytes[0] = (u8)g_battle_menu_thread_menu_data[g_battle_menu_current_id].selected_index;
        }
    }
}
