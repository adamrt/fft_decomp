#include "fft/battle_ai.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

void battle_ai_check_unit_for_crystal_or_treasure_status(void) {
    s32 unit_id;
    battle_stats_t* unit;

    g_battle_ai_unit_crystal_treasure_status = 0;
    unit_id = battle_ai_find_unit_at_coordinates(&g_battle_ai_data_base.current_action.coords);
    if (unit_id != BATTLE_UNIT_ID_NONE) {
        unit = &g_battle_unit_stats[unit_id];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
            if (unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRYSTAL)) {
                g_battle_ai_unit_crystal_treasure_status = 1;
            }
            if (unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TREASURE)) {
                g_battle_ai_unit_crystal_treasure_status = 2;
            }
        }
    }
}
