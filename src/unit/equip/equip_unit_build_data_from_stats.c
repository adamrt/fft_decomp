#include "fft/event_equip.h"
#include "psx/types.h"

/* Build EQUIP.OUT's unit preview from a unit_stats_t record, clamping the
 * displayed values and folding empty equipment slots (0xFF and above) to 0. */
void equip_unit_build_data_from_stats(unit_stats_t* stats, equip_unit_data_t* unit) {
    s32 i;
    s32 value;
    u8 larger;
    u8 other;
    u16 capped;

    memset(unit, 0, sizeof(equip_unit_data_t));
    unit->level = stats->level <= UNIT_LEVEL_MAX ? stats->level : UNIT_LEVEL_MAX;
    if (stats->team_flags & BATTLE_TEAM_MASK) {
        unit->team_kind = 1;
    }
    if (!(stats->team_flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK)) {
        unit->team_kind = 2;
    }
    i = action_find_unit_turn_index(stats);
    value = -1;
    if (i >= 0) {
        value = i + 1;
    }
    unit->list_index = value;
    unit->experience = stats->experience < UNIT_LEVEL_UP_EXPERIENCE ? stats->experience : UNIT_LEVEL_EXPERIENCE_MAX;
    unit->unit_index = stats->entd_slot;
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
    unit->identity.unit_index = stats->entd_slot;
    unit->identity.job_id = stats->job_id;
    unit->identity.brave = stats->brave;
    unit->identity.faith = stats->faith;
    unit->identity.zodiac = stats->birthday.zodiac;
    unit->identity.roster_slot = stats->formation_index;
    unit->move = stats->move;
    unit->speed = stats->attributes[UNIT_ATTRIBUTE_SPEED];
    unit->jump = stats->jump;
    unit->right_weapon_power = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    unit->left_weapon_power = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];
    unit->right_weapon_evade = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    unit->left_weapon_evade = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    unit->physical_attack = stats->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    unit->physical_class_evade = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    larger = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    other = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    if (larger < other) {
        larger = other;
    }
    unit->physical_shield_evade = larger;
    unit->magical_accessory_evade = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];
    unit->magic_attack = stats->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    unit->magical_class_evade = 0;
    larger = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    other = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    if (larger < other) {
        larger = other;
    }
    unit->magical_shield_evade = larger;
    unit->magical_accessory_evade = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];
    if (stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE) {
        unit->equipment[EQUIP_SLOT_RIGHT_HAND] = stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
    } else {
        unit->equipment[EQUIP_SLOT_RIGHT_HAND] = stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
    }
    if (stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE) {
        unit->equipment[EQUIP_SLOT_LEFT_HAND] = stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
    } else {
        unit->equipment[EQUIP_SLOT_LEFT_HAND] = stats->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
    }
    unit->equipment[EQUIP_SLOT_HEAD] = stats->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
    unit->equipment[EQUIP_SLOT_BODY] = stats->equipment[UNIT_EQUIPMENT_SLOT_BODY];
    unit->equipment[EQUIP_SLOT_ACCESSORY] = stats->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
    for (i = 0; i < EQUIP_SLOT_COUNT; i++) {
        if (unit->equipment[i] >= ITEM_ID_NONE) {
            unit->equipment[i] = 0;
        }
    }
    unit->party_index = stats->formation_index;
    bcopy(stats->equipment_categories, unit->equipment_categories, 4);
    bcopy(stats->ability_flags.support, unit->support_abilities, 4);
    unit->two_handing = equip_unit_is_two_handing_weapon(
        unit->equipment, stats->ability_flags.support[2] & BATTLE_SUPPORT_SET_3_TWO_HANDS);
}
