#include "fft/event_equip.h"
#include "psx/types.h"

/* Build the equipment data and HP/MP for `battle_id` and count the units still present on the field. */
void equip_unit_load_data_from_battle_stats(s32 battle_id) {
    battle_stats_t* stats;
    s32 i;
    s32 unit_count;
    u16 hp;
    u16 mp;

    stats = battle_unit_get_stats_from_battle_id(battle_id);
    equip_unit_build_data_from_battle_stats(stats, g_equip_unit_records);
    unit_count = 0;
    hp = stats->hp;
    mp = stats->mp;
    g_equip_unit_saved_hp = hp;
    g_equip_unit_saved_mp = mp;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        stats = battle_unit_get_stats_from_battle_id(i);
        if (stats == 0) {
            continue;
        }
        if (stats->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            continue;
        }
        if (stats->unit_flags & UNIT_FLAG_EGG) {
            unit_count++;
            continue;
        }
        if (stats->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRYSTAL)) {
            continue;
        }
        if (stats->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TREASURE)) {
            continue;
        }
        unit_count++;
    }

    g_equip_unit_records[0].unit_count = unit_count;
    g_equip_unit_records[0].battle_id = battle_id;
    g_equip_unit_data[0] = &g_equip_unit_records[0];
    g_equip_unit_data[1] = &g_equip_unit_records[1];
}
