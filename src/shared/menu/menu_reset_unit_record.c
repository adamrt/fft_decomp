#include "fft/menu.h"
#include "fft/unit.h"

/* Reset one unit's menu record while the unit is not under auto-battle control. */
void menu_reset_unit_record(s32 battle_id) {
    unit_stats_t* stats;
    menu_unit_selection_record_t* records;
    s32 i;

    stats = battle_unit_get_stats_from_battle_id(battle_id);
    records = g_menu_unit_selection_records;
    for (i = 0; i < 17; i++) {
        if (stats->auto_battle_setting == AI_AUTOBATTLE_MANUAL) {
            ((u8*)&records[battle_id])[i] = 0;
            g_menu_unit_selection_records[battle_id]._unknown_01 = 0xFF;
        }
    }
}
