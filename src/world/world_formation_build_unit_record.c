#include "fft/world.h"
#include "psx/types.h"

/*
 * Fills one WORLD formation record from a unit's battle stats and party data.
 *
 * HP/MP/level/experience/CT are clamped for display, monster-skillset units
 * (primary skillset 0xb0-0xdf) list up to three nonzero skillset abilities
 * instead of equipment and learned abilities, and JP totals cap at 9999.
 * One scratch variable `i` also carries the monster flag, the second evade
 * operand and the unlocked-job mask; separate locals reallocate a0/v1.
 */
void world_formation_build_unit_record(battle_stats_t* unit, world_formation_unit_t* record, party_data_t* party) {
    s16* abilities;
    s32 i;
    u32 evade;

    memset(record, 0, sizeof(world_formation_unit_t));
    record->level = party->level < 100 ? party->level : 99;
    if (unit->formation_index >= PARTY_GUEST_SLOT_FIRST) {
        record->guest_marker = 2;
    }
    record->experience = party->experience < 100 ? party->experience : 99;
    record->party_index = unit->formation_index;
    record->hp = unit->max_hp < 1000 ? unit->max_hp : 999;
    record->max_hp = unit->max_hp < 1000 ? unit->max_hp : 999;
    record->mp = unit->max_mp < 1000 ? unit->max_mp : 999;
    record->max_mp = unit->max_mp < 1000 ? unit->max_mp : 999;
    record->ct = unit->ct < 101 ? unit->ct : 100;
    record->max_ct = 100;
    *(s16*)record->name_index = unit->formation_index;
    record->job_id = party->job_id;
    record->brave = unit->brave;
    record->faith = unit->faith;
    record->zodiac = *(u16*)&unit->birthday >> 12;
    record->roster_slot = unit->formation_index;
    record->move = unit->move;
    record->speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    record->jump = unit->jump;
    record->right_weapon_power = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    record->left_weapon_power = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];
    record->right_weapon_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    record->left_weapon_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    i = (u8)(unit->primary_skillset + 0x50) < 0x30;
    record->uses_monster_skillset = i;
    record->physical_attack = unit->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    record->physical_class_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    i = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    if (evade < i) {
        evade = i;
    }
    record->physical_shield_evade = evade;
    record->physical_accessory_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE];
    record->magical_attack = unit->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    record->magical_class_evade = 0;
    evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    i = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    if (evade < i) {
        evade = i;
    }
    record->magical_shield_evade = evade;
    record->magical_accessory_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];

    if (record->uses_monster_skillset != 0) {
        s32 count;

        abilities = (s16*)main_ability_store_skillset_abilities(unit->primary_skillset, SKILLSET_ABILITY_FILTER_ACTION);
        count = 0;
        for (i = 0; i < 3; i++) {
            if (abilities[i] != 0) {
                (&record->primary_skillset)[count++] = abilities[i];
            }
        }
    } else {
        record->primary_skillset = unit->primary_skillset;
        if (record->job_id != JOB_ID_MIME) {
            record->equipment[0] = party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE
                ? party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]
                : party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
            record->equipment[1] = party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE
                ? party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]
                : party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
            record->equipment[2] = party->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
            record->equipment[3] = party->equipment[UNIT_EQUIPMENT_SLOT_BODY];
            record->equipment[4] = party->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
            for (i = 0; i < 5; i++) {
                if (record->equipment[i] >= 0xfe) {
                    record->equipment[i] = ITEM_ID_NOTHING;
                }
            }
            record->secondary_skillset = party->secondary_skillset;
            record->reaction_ability = party->reaction_ability[0] + (party->reaction_ability[1] << 8);
            record->support_ability = party->support_ability[0] + (party->support_ability[1] << 8);
            record->movement_ability = party->movement_ability[0] + (party->movement_ability[1] << 8);
        }
    }

    if (world_job_is_special_monster(unit->job_id) != 0 || record->uses_monster_skillset != 0) {
        bcopy(unit->unlocked_jobs, record->unlocked_jobs, sizeof(record->unlocked_jobs));
    } else {
        i = main_job_calculate_unlocked(unit->job_levels, unit->unit_flags & (UNIT_FLAG_FEMALE | UNIT_FLAG_MALE));
        record->unlocked_jobs[0] = i >> 16;
        record->unlocked_jobs[1] = i >> 8;
        record->unlocked_jobs[2] = i;
    }
    record->gender_flags = party->gender_flags;
    record->sprite_set = party->sprite_set;
    bcopy(unit->equipment_categories, record->equippable_item_types, sizeof(record->equippable_item_types));
    bcopy(party->learned_abilities, record->learned_abilities, sizeof(record->learned_abilities));
    bcopy(party->job_levels, record->job_levels, sizeof(record->job_levels));
    bcopy(party->job_points, record->job_points, sizeof(record->job_points));
    bcopy(party->total_job_points, record->total_job_points, sizeof(record->total_job_points));
    for (i = 0; i < JOB_ID_GENERIC_COUNT; i++) {
        if (record->job_points[i] >= 10000) {
            record->job_points[i] = 9999;
        }
        if (record->total_job_points[i] >= 10000) {
            record->total_job_points[i] = 9999;
        }
    }
    bcopy(party->name, record->name, sizeof(record->name));
    bcopy(unit->support_abilities, record->support_sets,
        sizeof(record->support_sets) + sizeof(record->support_sets_3) + sizeof(record->_unknown_127));
    record->proposition_status = party->proposition_status;
    record->graphic_variant = unit->graphic_variant;
    record->birthday = *(u16*)&unit->birthday & 0x1ff;
    record->egg_color = party->egg_color;
    record->two_handed
        = world_item_check_two_hands_for_weapons((struct weapon_pair*)record->equipment, record->support_sets_3 & 2);
}
