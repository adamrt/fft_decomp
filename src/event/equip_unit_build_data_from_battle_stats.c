#include "fft/event_equip.h"
#include "psx/types.h"

/* Build EQUIP.OUT's unit preview from a battle_stats_t record, clamping the
 * displayed values and folding empty equipment slots (0xFF and above) to 0. */
void equip_unit_build_data_from_battle_stats(battle_stats_t* stats, equip_unit_data_t* unit) {
    /* Pin: the target copies the call result to $a0; unpinned GCC tests $v0. */
    register s32 list_index asm("$4");
    s32 i;
    s32 value;
    u8 larger;
    u8 other;
    u16 capped;

    memset(unit, 0, sizeof(equip_unit_data_t));
    unit->level = stats->level < 100 ? stats->level : 99;
    if (stats->team_flags & BATTLE_TEAM_MASK) {
        unit->team_kind = 1;
    }
    if (!(stats->team_flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK)) {
        unit->team_kind = 2;
    }
    list_index = battle_action_check_at_list_for_unit_battle_id(stats);
    value = -1;
    if (list_index >= 0) {
        value = list_index + 1;
    }
    unit->list_index = value;
    unit->experience = stats->experience < 100 ? stats->experience : 99;
    unit->entd_slot_0a = stats->entd_slot;
    capped = 999;
    if (stats->hp < 1000) {
        capped = stats->hp;
    }
    unit->hp = capped;
    capped = 999;
    if (stats->max_hp < 1000) {
        capped = stats->max_hp;
    }
    unit->max_hp = capped;
    capped = 999;
    if (stats->mp < 1000) {
        capped = stats->mp;
    }
    unit->mp = capped;
    capped = 999;
    if (stats->max_mp < 1000) {
        capped = stats->max_mp;
    }
    unit->max_mp = capped;
    unit->ct = stats->has_turn != 1 ? stats->ct : 100;
    if (unit->ct > 100) {
        unit->ct = 100;
    }
    unit->max_ct = 100;
    unit->entd_slot = stats->entd_slot;
    unit->job_id = stats->job_id;
    unit->brave = stats->brave;
    unit->faith = stats->faith;
    unit->zodiac = stats->birthday.value >> 12;
    unit->formation_index = stats->formation_index;
    unit->move = stats->move;
    unit->speed = stats->attributes[UNIT_ATTRIBUTE_SPEED];
    unit->jump = stats->jump;
    unit->evasion_0_3[0] = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    unit->evasion_0_3[1] = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];
    unit->evasion_0_3[2] = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    unit->evasion_0_3[3] = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    unit->physical_attack = stats->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    unit->evasion_7 = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    larger = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    other = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    if (larger < other) {
        larger = other;
    }
    unit->evasion_5_6_max = larger;
    unit->evasion_8 = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];
    unit->magic_attack = stats->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    unit->magical_class_evade = 0;
    larger = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    other = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    if (larger < other) {
        larger = other;
    }
    unit->evasion_9_10_max = larger;
    unit->evasion_8 = stats->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];
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
        if (unit->equipment[i] >= 0xFF) {
            unit->equipment[i] = 0;
        }
    }
    unit->formation_index_78 = stats->formation_index;
    bcopy(stats->equipment_categories, unit->equipment_categories, 4);
    bcopy(stats->support_abilities, &unit->support_sets_1, 4);
    unit->two_handing = equip_unit_is_two_handing_weapon(
        (weapon_pair_t*)unit->equipment, stats->support_abilities[2] & BATTLE_SUPPORT_SET_3_TWO_HANDS);
}
