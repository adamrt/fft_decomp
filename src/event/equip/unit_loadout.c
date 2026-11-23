/* Two-handed weapon checks and unit data built from, loaded from and committed to the battle unit record. */

#include "fft/battle/at_list.h"
#include "fft/game/battle_unit.h"
#include "fft/game/item.h"
#include "fft/game/party.h"
#include "fft/game/status.h"
#include "fft/menu/menu.h"
#include "fft/party/equipment.h"
#include "fft/party/unit_panel.h"
#include "psx/libc.h"
#include "psx/types.h"

static s16 s_equipment_unit_saved_hp;

static s16 s_equipment_unit_saved_mp = 0;
/* Selected unit and its editable comparison copy; published through g_equipment_unit_data. */
static equipment_unit_preview_t s_equipment_unit_records[2] = { 0 };

/* Returns 1 when the unit holds a single weapon in two hands: exactly one of
 * the two hand slots is occupied, its item is below the throwable range, and
 * the weapon either forces two hands or allows them while `two_hands_support`
 * (the Two Hands support ability) is set. */
s32 equipment_is_two_handing_weapon(const s16* slots, s32 two_hands_support) {
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

/* Build EQUIP.OUT's unit preview from a battle_unit_t record, clamping the
 * displayed values and folding empty equipment slots (0xFF and above) to 0. */
static void build_unit_preview(battle_unit_t* battle_unit, equipment_unit_preview_t* unit) {
    s32 i;
    s32 value;
    u8 larger;
    u8 other;
    u16 capped;

    memset(unit, 0, sizeof(equipment_unit_preview_t));
    unit->level = battle_unit->level <= UNIT_LEVEL_MAX ? battle_unit->level : UNIT_LEVEL_MAX;
    if (battle_unit->team_flags & UNIT_TEAM_MASK) {
        unit->team_kind = 1;
    }
    if (!(battle_unit->team_flags & UNIT_TEAM_OR_PLAYER_CONTROL_MASK)) {
        unit->team_kind = 2;
    }
    i = at_list_find_unit(battle_unit);
    value = -1;
    if (i >= 0) {
        value = i + 1;
    }
    unit->list_index = value;
    unit->experience
        = battle_unit->experience < UNIT_LEVEL_UP_EXPERIENCE ? battle_unit->experience : UNIT_LEVEL_EXPERIENCE_MAX;
    unit->unit_index = battle_unit->active_battle_unit_id;
    capped = UNIT_HP_MP_MAX;
    if (battle_unit->hp <= UNIT_HP_MP_MAX) {
        capped = battle_unit->hp;
    }
    unit->gauges[MENU_UNIT_STATUS_GAUGE_HP].value = capped;
    capped = UNIT_HP_MP_MAX;
    if (battle_unit->max_hp <= UNIT_HP_MP_MAX) {
        capped = battle_unit->max_hp;
    }
    unit->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = capped;
    capped = UNIT_HP_MP_MAX;
    if (battle_unit->mp <= UNIT_HP_MP_MAX) {
        capped = battle_unit->mp;
    }
    unit->gauges[MENU_UNIT_STATUS_GAUGE_MP].value = capped;
    capped = UNIT_HP_MP_MAX;
    if (battle_unit->max_mp <= UNIT_HP_MP_MAX) {
        capped = battle_unit->max_mp;
    }
    unit->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = capped;
    unit->gauges[MENU_UNIT_STATUS_GAUGE_CT].value
        = battle_unit->has_turn != 1 ? battle_unit->ct : UNIT_TURN_CT_THRESHOLD;
    if (unit->gauges[MENU_UNIT_STATUS_GAUGE_CT].value > UNIT_TURN_CT_THRESHOLD) {
        unit->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = UNIT_TURN_CT_THRESHOLD;
    }
    unit->gauges[MENU_UNIT_STATUS_GAUGE_CT].max = UNIT_TURN_CT_THRESHOLD;
    unit->identity.unit_index = battle_unit->active_battle_unit_id;
    unit->identity.job_id = battle_unit->job_id;
    unit->identity.brave = battle_unit->brave;
    unit->identity.faith = battle_unit->faith;
    unit->identity.zodiac = battle_unit->birthday.zodiac;
    unit->identity.roster_slot = battle_unit->formation_index;
    unit->move = battle_unit->move;
    unit->speed = battle_unit->attributes[UNIT_ATTRIBUTE_SPEED];
    unit->jump = battle_unit->jump;
    unit->right_weapon_power = battle_unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    unit->left_weapon_power = battle_unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];
    unit->right_weapon_evade = battle_unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    unit->left_weapon_evade = battle_unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    unit->physical_attack = battle_unit->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    unit->physical_class_evade = battle_unit->equipment_stats[UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    larger = battle_unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    other = battle_unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    if (larger < other) {
        larger = other;
    }
    unit->physical_shield_evade = larger;
    /* Retail stores the magic accessory evade here where the physical one belongs, so
     * physical_accessory_evade stays 0 (QUIRKS.md). */
    unit->magical_accessory_evade = battle_unit->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];
    unit->magic_attack = battle_unit->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    unit->magical_class_evade = 0;
    larger = battle_unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    other = battle_unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    if (larger < other) {
        larger = other;
    }
    unit->magical_shield_evade = larger;
    unit->magical_accessory_evade = battle_unit->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];
    if (battle_unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE) {
        unit->equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND] = battle_unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
    } else {
        unit->equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND] = battle_unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
    }
    if (battle_unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE) {
        unit->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND] = battle_unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
    } else {
        unit->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND] = battle_unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
    }
    unit->equipment[MENU_EQUIPMENT_SLOT_HEAD] = battle_unit->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
    unit->equipment[MENU_EQUIPMENT_SLOT_BODY] = battle_unit->equipment[UNIT_EQUIPMENT_SLOT_BODY];
    unit->equipment[MENU_EQUIPMENT_SLOT_ACCESSORY] = battle_unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
    for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
        if (unit->equipment[i] >= ITEM_ID_NONE) {
            unit->equipment[i] = 0;
        }
    }
    unit->party_index = battle_unit->formation_index;
    bcopy(battle_unit->equipment_categories, unit->equipment_categories, sizeof(unit->equipment_categories));
    bcopy(battle_unit->ability_flags.support, unit->support_abilities, sizeof(unit->support_abilities));
    unit->two_handing = equipment_is_two_handing_weapon(
        unit->equipment, battle_unit->ability_flags.support[2] & UNIT_SUPPORT_SET_3_TWO_HANDS);
}

/* Build the equipment data and HP/MP for `battle_unit_id` and count the units still on the field
 * (eggs included, crystals and treasure chests not). */
void equipment_load_unit(s32 battle_unit_id) {
    battle_unit_t* battle_unit = battle_unit_get(battle_unit_id);
    s32 i;
    s32 unit_count;
    u16 hp;
    u16 mp;

    build_unit_preview(battle_unit, s_equipment_unit_records);
    unit_count = 0;
    hp = battle_unit->hp;
    mp = battle_unit->mp;
    s_equipment_unit_saved_hp = hp;
    s_equipment_unit_saved_mp = mp;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        battle_unit = battle_unit_get(i);
        if (battle_unit == 0) {
            continue;
        }
        if (battle_unit->active_battle_unit_id == UNIT_ACTIVE_BATTLE_UNIT_ID_NONE) {
            continue;
        }
        if (battle_unit->unit_flags & UNIT_FLAG_EGG) {
            unit_count++;
            continue;
        }
        if (battle_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
            & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL)) {
            continue;
        }
        if (battle_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
            & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
            continue;
        }
        unit_count++;
    }

    s_equipment_unit_records[0].unit_count = unit_count;
    s_equipment_unit_records[0].battle_unit_id = battle_unit_id;
    g_equipment_unit_data[0] = &s_equipment_unit_records[0];
    g_equipment_unit_data[1] = &s_equipment_unit_records[1];
}

/* Write the edited loadout back to the unit's battle stats and party record (a
 * hand's item goes to the hand's shield slot when it is a shield, else to its weapon
 * slot), refresh its stats, restore HP/MP up to the new maxima and rebuild the
 * EQUIP unit record. */
void equipment_commit_loadout(void) {
    unit_panel_record_t bonuses;
    battle_unit_t* battle_unit;
    s16 unit_id = g_equipment_unit_data[0]->battle_unit_id;
    s32 item_id;
    s32 saved;
    s32 hp;
    s32 mp;
    s32 max;

    battle_unit = battle_unit_get(unit_id);
    item_id = g_equipment_unit_data[0]->equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND];
    battle_unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
    battle_unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = ITEM_ID_NONE;
    if (item_id != 0) {
        if (g_item_primary_data[item_id].type == ITEM_TYPE_SHIELD) {
            battle_unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = item_id;
        } else {
            battle_unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = item_id;
        }
    }
    item_id = g_equipment_unit_data[0]->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND];
    battle_unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
    battle_unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = ITEM_ID_NONE;
    if (item_id != 0) {
        if (g_item_primary_data[item_id].type == ITEM_TYPE_SHIELD) {
            battle_unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = item_id;
        } else {
            battle_unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = item_id;
        }
    }
    battle_unit->equipment[UNIT_EQUIPMENT_SLOT_HEAD]
        = g_equipment_unit_data[0]->equipment[MENU_EQUIPMENT_SLOT_HEAD] != 0
        ? g_equipment_unit_data[0]->equipment[MENU_EQUIPMENT_SLOT_HEAD]
        : ITEM_ID_NONE;
    battle_unit->equipment[UNIT_EQUIPMENT_SLOT_BODY]
        = g_equipment_unit_data[0]->equipment[MENU_EQUIPMENT_SLOT_BODY] != 0
        ? g_equipment_unit_data[0]->equipment[MENU_EQUIPMENT_SLOT_BODY]
        : ITEM_ID_NONE;
    battle_unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]
        = g_equipment_unit_data[0]->equipment[MENU_EQUIPMENT_SLOT_ACCESSORY] != 0
        ? g_equipment_unit_data[0]->equipment[MENU_EQUIPMENT_SLOT_ACCESSORY]
        : ITEM_ID_NONE;
    bcopy(battle_unit->equipment, party_unit_get(g_equipment_unit_data[0]->party_index)->equipment,
        sizeof(battle_unit->equipment));
    battle_unit_refresh_stats(battle_unit);
    battle_unit = battle_unit_get(unit_id);
    hp = s_equipment_unit_saved_hp;
    mp = s_equipment_unit_saved_mp;
    max = battle_unit->max_hp;
    if (hp > max) {
        hp = max;
    }
    battle_unit->hp = hp;
    max = battle_unit->max_mp;
    battle_unit->mp = mp > max ? max : mp;
    equipment_build_weapon_evade_totals(&bonuses, g_equipment_unit_data[0]->equipment);
    battle_unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER] = bonuses.state.right_weapon_power;
    battle_unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER] = bonuses.state.left_weapon_power;
    battle_unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE] = bonuses.state.right_weapon_evade;
    battle_unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE] = bonuses.state.left_weapon_evade;
    saved = g_equipment_unit_data[0]->unit_count;
    build_unit_preview(battle_unit, g_equipment_unit_data[0]);
    g_equipment_unit_data[0]->unit_count = saved;
    g_equipment_unit_data[0]->battle_unit_id = unit_id;
}
