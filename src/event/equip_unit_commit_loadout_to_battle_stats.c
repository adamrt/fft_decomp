#include "fft/event_equip.h"
#include "psx/libc.h"
#include "psx/types.h"

void equip_unit_commit_loadout_to_battle_stats(void) {
    u8 scratch[0x48];
    battle_stats_t* stats;
    s32 unit_id;
    s32 item_id;
    s32 value;
    s32 saved;

    {
        /* Pin: the target loads the id into $a0 first; unpinned GCC loads it into $s2. */
        register s32 arg __asm__("$4");
        arg = g_equip_unit_data[0]->battle_id;
        unit_id = arg;
        stats = battle_unit_get_stats_from_battle_id(arg);
    }
    item_id = g_equip_unit_data[0]->equipment[0];
    stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = 0xFF;
    stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = 0xFF;
    if (item_id != 0) {
        if (g_main_item_primary_data[item_id].type == ITEM_TYPE_SHIELD) {
            stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = item_id;
        } else {
            stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = item_id;
        }
    }
    item_id = g_equip_unit_data[0]->equipment[1];
    stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = 0xFF;
    stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = 0xFF;
    if (item_id != 0) {
        if (g_main_item_primary_data[item_id].type == ITEM_TYPE_SHIELD) {
            stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = item_id;
        } else {
            stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = item_id;
        }
    }
    value = 0xFF;
    if (g_equip_unit_data[0]->equipment[2] != 0) {
        value = *(u8*)&g_equip_unit_data[0]->equipment[2];
    }
    stats->equipment[UNIT_EQUIPMENT_SLOT_HEAD] = value;
    value = 0xFF;
    if (g_equip_unit_data[0]->equipment[3] != 0) {
        value = *(u8*)&g_equip_unit_data[0]->equipment[3];
    }
    stats->equipment[UNIT_EQUIPMENT_SLOT_BODY] = value;
    value = 0xFF;
    if (g_equip_unit_data[0]->equipment[4] != 0) {
        value = *(u8*)&g_equip_unit_data[0]->equipment[4];
    }
    stats->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY] = value;
    bcopy(stats->equipment, main_party_get_data_pointer(g_equip_unit_data[0]->formation_index_78)->equipment, 7);
    main_unit_refresh_stats_and_statuses(stats);
    stats = battle_unit_get_stats_from_battle_id(unit_id);
    {
        s32 cap = g_equip_unit_saved_hp;
        s32 current = stats->max_hp;
        /* Pin: unpinned GCC rotates the three cap registers ($a1, $a0, $v1). */
        register s32 mp_cap __asm__("$3");
        mp_cap = g_equip_unit_saved_mp;
        if (current < cap) {
            cap = current;
        }
        stats->hp = cap;
        current = stats->max_mp;
        if (current < mp_cap) {
            mp_cap = current;
        }
        stats->mp = mp_cap;
    }
    /* The target frame reserves only 0x48 bytes here; an equip_stats_t local (0x80) would grow it. */
    equip_unit_calculate_equipment_stat_bonuses((equip_stats_t*)scratch, g_equip_unit_data[0]->equipment);
    stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER] = scratch[6];
    stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER] = scratch[8];
    stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE] = scratch[10];
    stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE] = scratch[12];
    saved = g_equip_unit_data[0]->unit_count;
    equip_unit_build_data_from_battle_stats(stats, g_equip_unit_data[0]);
    g_equip_unit_data[0]->unit_count = saved;
    g_equip_unit_data[0]->battle_id = unit_id;
}
