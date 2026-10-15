#include "fft/event_equip.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Write the edited loadout back to the unit's battle stats and party record,
 * refresh its stats, restore HP/MP up to the new maxima and rebuild the
 * EQUIP unit record. */
void equip_unit_commit_loadout_to_stats(void) {
    menu_status_panel_slot_storage_t bonuses;
    unit_stats_t* stats;
    s16 unit_id;
    s32 item_id;
    s32 saved;
    s32 hp;
    s32 mp;
    s32 max;

    unit_id = g_equip_unit_data[0]->battle_id;
    stats = battle_unit_get_stats_from_battle_id(unit_id);
    item_id = g_equip_unit_data[0]->equipment[EQUIP_SLOT_RIGHT_HAND];
    stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
    stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = ITEM_ID_NONE;
    if (item_id != 0) {
        if (g_main_item_primary_data[item_id].type == ITEM_TYPE_SHIELD) {
            stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = item_id;
        } else {
            stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = item_id;
        }
    }
    item_id = g_equip_unit_data[0]->equipment[EQUIP_SLOT_LEFT_HAND];
    stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
    stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = ITEM_ID_NONE;
    if (item_id != 0) {
        if (g_main_item_primary_data[item_id].type == ITEM_TYPE_SHIELD) {
            stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = item_id;
        } else {
            stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = item_id;
        }
    }
    stats->equipment[UNIT_EQUIPMENT_SLOT_HEAD] = g_equip_unit_data[0]->equipment[EQUIP_SLOT_HEAD] != 0
        ? g_equip_unit_data[0]->equipment[EQUIP_SLOT_HEAD]
        : ITEM_ID_NONE;
    stats->equipment[UNIT_EQUIPMENT_SLOT_BODY] = g_equip_unit_data[0]->equipment[EQUIP_SLOT_BODY] != 0
        ? g_equip_unit_data[0]->equipment[EQUIP_SLOT_BODY]
        : ITEM_ID_NONE;
    stats->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY] = g_equip_unit_data[0]->equipment[EQUIP_SLOT_ACCESSORY] != 0
        ? g_equip_unit_data[0]->equipment[EQUIP_SLOT_ACCESSORY]
        : ITEM_ID_NONE;
    bcopy(stats->equipment, main_party_get_unit(g_equip_unit_data[0]->party_index)->equipment, 7);
    main_unit_refresh_stats_and_statuses(stats);
    stats = battle_unit_get_stats_from_battle_id(unit_id);
    hp = g_equip_unit_saved_hp;
    mp = g_equip_unit_saved_mp;
    max = stats->max_hp;
    if (hp > max) {
        hp = max;
    }
    stats->hp = hp;
    max = stats->max_mp;
    stats->mp = mp > max ? max : mp;
    item_sum_equipment_stat_details(&bonuses, g_equip_unit_data[0]->equipment);
    stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER] = bonuses.state.right_weapon_power;
    stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER] = bonuses.state.left_weapon_power;
    stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE] = bonuses.state.right_weapon_evade;
    stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE] = bonuses.state.left_weapon_evade;
    saved = g_equip_unit_data[0]->unit_count;
    equip_unit_build_data_from_stats(stats, g_equip_unit_data[0]);
    g_equip_unit_data[0]->unit_count = saved;
    g_equip_unit_data[0]->battle_id = unit_id;
}
