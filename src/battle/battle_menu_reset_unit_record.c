#include "fft/battle.h"

/* Reset one unit's menu record while the unit is not under auto-battle control. */
void battle_menu_reset_unit_record(s32 battle_id) {
    battle_stats_t* stats;
    battle_menu_record_t* records;
    s32 i;

    stats = battle_unit_get_stats_from_battle_id(battle_id);
    records = g_battle_menu_unit_selection_records;
    for (i = 0; i < 17; i++) {
        if (stats->auto_battle_setting == 0) {
            records[battle_id].bytes[i] = 0;
            g_battle_menu_unit_selection_records[battle_id].bytes[1] = 0xFF;
        }
    }
}
