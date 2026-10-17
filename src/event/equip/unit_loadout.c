/* Two-handed weapon checks and unit data built from, loaded from and committed to stats. */

#include "fft/at_list.h"
#include "fft/equipment.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/party.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "fft/unit_runtime.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Selected unit and its editable comparison copy; published through g_equipment_unit_data. */
static equipment_unit_preview_t g_equipment_unit_records[2] = { 0 };

/* Returns 1 when the unit holds a single weapon in two hands: exactly one of
 * the two hand slots is occupied, its item is below the throwable range, and
 * the weapon either forces two hands or allows them while `two_hands_support`
 * (the Two Hands support ability) is set. */
s32 unit_is_two_handing_weapon(const s16* slots, s32 two_hands_support) {
    s32 result = 0;
    s32 item_id;
    s16 w0 = slots[MENU_EQUIPMENT_SLOT_RIGHT_HAND];
    s16 w1;
    u8 flags;

    item_id = 0;
    if (w0 != 0) {
        if (slots[MENU_EQUIPMENT_SLOT_LEFT_HAND] == 0) {
            item_id = w0;
        }
    } else {
        w1 = slots[MENU_EQUIPMENT_SLOT_LEFT_HAND];
        if (w1 != 0) {
            item_id = w1;
        }
    }
    if (item_id != 0 && item_id < ITEM_ID_THROWABLE_FIRST) {
        flags = g_item_weapon_data[item_id].flags;
        if (flags & WEAPON_FLAG_FORCED_TWO_HANDS) {
            result = 1;
        } else if (two_hands_support != 0) {
            if (flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE) {
                result = 1;
            }
        }
    }
    return result;
}

/* Build EQUIP.OUT's unit preview from a unit_stats_t record, clamping the
 * displayed values and folding empty equipment slots (0xFF and above) to 0. */
static void equipment_build_unit_preview(unit_stats_t* stats, equipment_unit_preview_t* unit) {
    s32 i;
    s32 value;
    u8 larger;
    u8 other;
    u16 capped;

    memset(unit, 0, sizeof(equipment_unit_preview_t));
    unit->level = stats->level <= UNIT_LEVEL_MAX ? stats->level : UNIT_LEVEL_MAX;
    if (stats->team_flags & UNIT_TEAM_MASK) {
        unit->team_kind = 1;
    }
    if (!(stats->team_flags & UNIT_TEAM_OR_PLAYER_CONTROL_MASK)) {
        unit->team_kind = 2;
    }
    i = at_list_find_unit(stats);
    value = -1;
    if (i >= 0) {
        value = i + 1;
    }
    unit->list_index = value;
    unit->experience = stats->experience < UNIT_LEVEL_UP_EXPERIENCE ? stats->experience : UNIT_LEVEL_EXPERIENCE_MAX;
    unit->unit_index = stats->active_battle_id;
    capped = UNIT_HP_MP_MAX;
    if (stats->hp <= UNIT_HP_MP_MAX) {
        capped = stats->hp;
    }
    unit->gauges[MENU_UNIT_STATUS_GAUGE_HP].value = capped;
    capped = UNIT_HP_MP_MAX;
    if (stats->max_hp <= UNIT_HP_MP_MAX) {
        capped = stats->max_hp;
    }
    unit->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = capped;
    capped = UNIT_HP_MP_MAX;
    if (stats->mp <= UNIT_HP_MP_MAX) {
        capped = stats->mp;
    }
    unit->gauges[MENU_UNIT_STATUS_GAUGE_MP].value = capped;
    capped = UNIT_HP_MP_MAX;
    if (stats->max_mp <= UNIT_HP_MP_MAX) {
        capped = stats->max_mp;
    }
    unit->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = capped;
    unit->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = stats->has_turn != 1 ? stats->ct : UNIT_TURN_CT_THRESHOLD;
    if (unit->gauges[MENU_UNIT_STATUS_GAUGE_CT].value > UNIT_TURN_CT_THRESHOLD) {
        unit->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = UNIT_TURN_CT_THRESHOLD;
    }
    unit->gauges[MENU_UNIT_STATUS_GAUGE_CT].max = UNIT_TURN_CT_THRESHOLD;
    unit->identity.unit_index = stats->active_battle_id;
    unit->identity.job_id = stats->job_id;
    unit->identity.brave = stats->brave;
    unit->identity.faith = stats->faith;
    unit->identity.zodiac = stats->birthday.zodiac;
    unit->identity.roster_slot = stats->formation_index;
    unit->move = stats->move;
    unit->speed = stats->attributes[UNIT_ATTRIBUTE_SPEED];
    unit->jump = stats->jump;
    unit->right_weapon_power = stats->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    unit->left_weapon_power = stats->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];
    unit->right_weapon_evade = stats->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    unit->left_weapon_evade = stats->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    unit->physical_attack = stats->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    unit->physical_class_evade = stats->equipment_stats[UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    larger = stats->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    other = stats->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    if (larger < other) {
        larger = other;
    }
    unit->physical_shield_evade = larger;
    /* Retail stores the magic accessory evade here where the physical one belongs, so
     * physical_accessory_evade stays 0 (QUIRKS.md). */
    unit->magical_accessory_evade = stats->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];
    unit->magic_attack = stats->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    unit->magical_class_evade = 0;
    larger = stats->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    other = stats->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    if (larger < other) {
        larger = other;
    }
    unit->magical_shield_evade = larger;
    unit->magical_accessory_evade = stats->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];
    if (stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE) {
        unit->equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND] = stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
    } else {
        unit->equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND] = stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
    }
    if (stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE) {
        unit->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND] = stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
    } else {
        unit->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND] = stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
    }
    unit->equipment[MENU_EQUIPMENT_SLOT_HEAD] = stats->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
    unit->equipment[MENU_EQUIPMENT_SLOT_BODY] = stats->equipment[UNIT_EQUIPMENT_SLOT_BODY];
    unit->equipment[MENU_EQUIPMENT_SLOT_ACCESSORY] = stats->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
    for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
        if (unit->equipment[i] >= ITEM_ID_NONE) {
            unit->equipment[i] = 0;
        }
    }
    unit->party_index = stats->formation_index;
    bcopy(stats->equipment_categories, unit->equipment_categories, sizeof(unit->equipment_categories));
    bcopy(stats->ability_flags.support, unit->support_abilities, sizeof(unit->support_abilities));
    unit->two_handing
        = unit_is_two_handing_weapon(unit->equipment, stats->ability_flags.support[2] & UNIT_SUPPORT_SET_3_TWO_HANDS);
}

/* Build the equipment data and HP/MP for `battle_id` and count the units still on the field
 * (eggs included, crystals and treasure chests not). */
void equipment_load_unit(s32 battle_id) {
    unit_stats_t* stats = unit_get_stats(battle_id);
    s32 i;
    s32 unit_count;
    u16 hp;
    u16 mp;

    equipment_build_unit_preview(stats, g_equipment_unit_records);
    unit_count = 0;
    hp = stats->hp;
    mp = stats->mp;
    g_equipment_unit_saved_hp = hp;
    g_equipment_unit_saved_mp = mp;

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        stats = unit_get_stats(i);
        if (stats == 0) {
            continue;
        }
        if (stats->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
            continue;
        }
        if (stats->unit_flags & UNIT_FLAG_EGG) {
            unit_count++;
            continue;
        }
        if (stats->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL)) {
            continue;
        }
        if (stats->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
            continue;
        }
        unit_count++;
    }

    g_equipment_unit_records[0].unit_count = unit_count;
    g_equipment_unit_records[0].battle_id = battle_id;
    g_equipment_unit_data[0] = &g_equipment_unit_records[0];
    g_equipment_unit_data[1] = &g_equipment_unit_records[1];
}

/* Write the edited loadout back to the unit's battle stats and party record (a
 * hand's item goes to the hand's shield slot when it is a shield, else to its weapon
 * slot), refresh its stats, restore HP/MP up to the new maxima and rebuild the
 * EQUIP unit record. */
void equipment_commit_loadout(void) {
    unit_panel_record_t bonuses;
    unit_stats_t* stats;
    s16 unit_id = g_equipment_unit_data[0]->battle_id;
    s32 item_id;
    s32 saved;
    s32 hp;
    s32 mp;
    s32 max;

    stats = unit_get_stats(unit_id);
    item_id = g_equipment_unit_data[0]->equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND];
    stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
    stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = ITEM_ID_NONE;
    if (item_id != 0) {
        if (g_item_primary_data[item_id].type == ITEM_TYPE_SHIELD) {
            stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = item_id;
        } else {
            stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = item_id;
        }
    }
    item_id = g_equipment_unit_data[0]->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND];
    stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
    stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = ITEM_ID_NONE;
    if (item_id != 0) {
        if (g_item_primary_data[item_id].type == ITEM_TYPE_SHIELD) {
            stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = item_id;
        } else {
            stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = item_id;
        }
    }
    stats->equipment[UNIT_EQUIPMENT_SLOT_HEAD] = g_equipment_unit_data[0]->equipment[MENU_EQUIPMENT_SLOT_HEAD] != 0
        ? g_equipment_unit_data[0]->equipment[MENU_EQUIPMENT_SLOT_HEAD]
        : ITEM_ID_NONE;
    stats->equipment[UNIT_EQUIPMENT_SLOT_BODY] = g_equipment_unit_data[0]->equipment[MENU_EQUIPMENT_SLOT_BODY] != 0
        ? g_equipment_unit_data[0]->equipment[MENU_EQUIPMENT_SLOT_BODY]
        : ITEM_ID_NONE;
    stats->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]
        = g_equipment_unit_data[0]->equipment[MENU_EQUIPMENT_SLOT_ACCESSORY] != 0
        ? g_equipment_unit_data[0]->equipment[MENU_EQUIPMENT_SLOT_ACCESSORY]
        : ITEM_ID_NONE;
    bcopy(stats->equipment, party_get_unit(g_equipment_unit_data[0]->party_index)->equipment, sizeof(stats->equipment));
    unit_refresh_stats_and_statuses(stats);
    stats = unit_get_stats(unit_id);
    hp = g_equipment_unit_saved_hp;
    mp = g_equipment_unit_saved_mp;
    max = stats->max_hp;
    if (hp > max) {
        hp = max;
    }
    stats->hp = hp;
    max = stats->max_mp;
    stats->mp = mp > max ? max : mp;
    equipment_sum_weapon_and_evade_bonuses(&bonuses, g_equipment_unit_data[0]->equipment);
    stats->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER] = bonuses.state.right_weapon_power;
    stats->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER] = bonuses.state.left_weapon_power;
    stats->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE] = bonuses.state.right_weapon_evade;
    stats->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE] = bonuses.state.left_weapon_evade;
    saved = g_equipment_unit_data[0]->unit_count;
    equipment_build_unit_preview(stats, g_equipment_unit_data[0]);
    g_equipment_unit_data[0]->unit_count = saved;
    g_equipment_unit_data[0]->battle_id = unit_id;
}
