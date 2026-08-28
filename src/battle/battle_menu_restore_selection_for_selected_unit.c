#include "fft/battle.h"

/* Five-byte remap rows at 0x8016923c: when the menu id matches and the unit's
 * stored selection is one of the three candidates, the selection becomes
 * replacement. */
typedef struct battle_menu_selection_remap {
    /* 0x00 menu id, 0x01 replacement selection, 0x02..0x04 the three
     * candidate selections it replaces. */
    u8 bytes[5];
} battle_menu_selection_remap_t;

typedef char assert_battle_menu_selection_remap_size[sizeof(battle_menu_selection_remap_t) == 5 ? 1 : -1];

extern battle_menu_selection_remap_t g_battle_menu_selection_remaps[8];

void battle_menu_restore_selection_for_selected_unit(s32 menu_id) {
    battle_menu_record_t* record;
    s32 i;
    s32 row;
    s32 candidate;

    for (i = 0; i < 12; i++) {
        if (g_battle_menu_id_records[i].menu_id == menu_id) {
            break;
        }
    }
    if (i == 12) {
        return;
    }
    record = &g_battle_menu_unit_selection_records[g_battle_active_turn_unit.battle_id];
    for (row = 0; row < 8; row++) {
        if (menu_id == g_battle_menu_selection_remaps[row].bytes[0]) {
            for (candidate = 2; candidate < 5; candidate++) {
                if (record->bytes[0] == g_battle_menu_selection_remaps[row].bytes[candidate]) {
                    record->bytes[0] = g_battle_menu_selection_remaps[row].bytes[1];
                    break;
                }
            }
        }
    }
    g_battle_menu_thread_menu_data[menu_id].selected_index = record->bytes[0];
}
