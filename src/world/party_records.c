/* Unit records built from the party, the record list and saving records back to the party. */

#include "fft/data.h"
#include "fft/main.h"
#include "fft/main_item.h"
#include "fft/main_unit.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "fft/world_formation.h"
#include "psx/libc.h"
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
void world_formation_build_unit_record(const unit_stats_t* unit, world_unit_t* record, const party_unit_t* party) {
    s16* abilities;
    s32 i;
    u32 evade;

    memset(record, 0, sizeof(world_unit_t));
    record->status.level = party->level <= UNIT_LEVEL_MAX ? party->level : UNIT_LEVEL_MAX;
    if (unit->formation_index >= PARTY_GUEST_SLOT_FIRST) {
        record->status.team_kind = 2;
    }
    record->status.experience
        = party->experience < UNIT_LEVEL_UP_EXPERIENCE ? party->experience : UNIT_LEVEL_EXPERIENCE_MAX;
    record->status.unit_index = unit->formation_index;
    record->status.gauges[MENU_UNIT_STATUS_GAUGE_HP].value
        = unit->max_hp <= UNIT_HP_MP_MAX ? unit->max_hp : UNIT_HP_MP_MAX;
    record->status.gauges[MENU_UNIT_STATUS_GAUGE_HP].max
        = unit->max_hp <= UNIT_HP_MP_MAX ? unit->max_hp : UNIT_HP_MP_MAX;
    record->status.gauges[MENU_UNIT_STATUS_GAUGE_MP].value
        = unit->max_mp <= UNIT_HP_MP_MAX ? unit->max_mp : UNIT_HP_MP_MAX;
    record->status.gauges[MENU_UNIT_STATUS_GAUGE_MP].max
        = unit->max_mp <= UNIT_HP_MP_MAX ? unit->max_mp : UNIT_HP_MP_MAX;
    record->status.gauges[MENU_UNIT_STATUS_GAUGE_CT].value
        = unit->ct <= UNIT_TURN_CT_THRESHOLD ? unit->ct : UNIT_TURN_CT_THRESHOLD;
    record->status.gauges[MENU_UNIT_STATUS_GAUGE_CT].max = UNIT_TURN_CT_THRESHOLD;
    record->identity.unit_index = unit->formation_index;
    record->identity.job_id = party->job_id;
    record->identity.brave = unit->brave;
    record->identity.faith = unit->faith;
    record->identity.zodiac = unit->birthday.zodiac;
    record->identity.roster_slot = unit->formation_index;
    record->move = unit->move;
    record->speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    record->jump = unit->jump;
    record->right_weapon_power = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    record->left_weapon_power = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];
    record->right_weapon_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    record->left_weapon_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    i = unit->primary_skillset >= SKILLSET_ID_MONSTER_FIRST && unit->primary_skillset < SKILLSET_ID_END;
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
                record->abilities.slots[count++] = abilities[i];
            }
        }
    } else {
        record->abilities.fields.primary_skillset = unit->primary_skillset;
        if (record->identity.job_id != JOB_ID_MIME) {
            record->equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND]
                = party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE
                ? party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]
                : party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
            record->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND]
                = party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE
                ? party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]
                : party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
            record->equipment[MENU_EQUIPMENT_SLOT_HEAD] = party->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
            record->equipment[MENU_EQUIPMENT_SLOT_BODY] = party->equipment[UNIT_EQUIPMENT_SLOT_BODY];
            record->equipment[MENU_EQUIPMENT_SLOT_ACCESSORY] = party->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
            for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                if (record->equipment[i] >= ITEM_ID_END) {
                    record->equipment[i] = ITEM_ID_NOTHING;
                }
            }
            record->abilities.fields.secondary_skillset = party->secondary_skillset;
            record->abilities.fields.reaction_ability = party->reaction_ability[0] + (party->reaction_ability[1] << 8);
            record->abilities.fields.support_ability = party->support_ability[0] + (party->support_ability[1] << 8);
            record->abilities.fields.movement_ability = party->movement_ability[0] + (party->movement_ability[1] << 8);
        }
    }

    if (job_is_special_monster(unit->job_id) != 0 || record->uses_monster_skillset != 0) {
        bcopy(unit->career.unlocked_jobs, record->unlocked_jobs, sizeof(record->unlocked_jobs));
    } else {
        i = main_job_calculate_unlocked(
            unit->career.job_levels, unit->unit_flags & (UNIT_FLAG_FEMALE | UNIT_FLAG_MALE));
        record->unlocked_jobs[0] = i >> 16;
        record->unlocked_jobs[1] = i >> 8;
        record->unlocked_jobs[2] = i;
    }
    record->gender_flags = party->gender_flags;
    record->sprite_set = party->sprite_set;
    bcopy(unit->equipment_categories, record->equippable_item_types, sizeof(record->equippable_item_types));
    bcopy(party->career.learned_abilities, record->learned_abilities, sizeof(record->learned_abilities));
    bcopy(party->career.job_levels, record->job_levels, sizeof(record->job_levels));
    bcopy(&party->career.job_points.current, &record->job_points.current, sizeof(record->job_points.current));
    bcopy(&party->career.job_points.total, &record->job_points.total, sizeof(record->job_points.total));
    for (i = 0; i < JOB_ID_GENERIC_COUNT; i++) {
        if (record->job_points.current.values[i] >= 10000) {
            record->job_points.current.values[i] = 9999;
        }
        if (record->job_points.total.values[i] >= 10000) {
            record->job_points.total.values[i] = 9999;
        }
    }
    bcopy(party->name, record->name, sizeof(record->name));
    bcopy(unit->ability_flags.support, record->support_abilities, sizeof(record->support_abilities));
    record->proposition_status = party->proposition_status;
    record->monster_graphic = unit->monster_graphic;
    record->birthday = unit->birthday.birthday;
    record->egg_color = party->egg_color;
    record->two_handed = unit_is_two_handing_weapon(
        (s16*)record->equipment, record->support_abilities[2] & BATTLE_SUPPORT_SET_3_TWO_HANDS);
}

/*
 * Rebuilds the compacted WORLD formation records from the 20 roster slots.
 *
 * Mode 1 drops units with a proposition in progress and the egg-unit flag, a
 * monster skillset, or a special-monster job; mode 2 excludes eggs
 * and human units. A nonzero item id keeps only
 * units with that item equipped. The list argument is unused here; the sorter
 * at 0x80121c60 fills g_world_unit_pointers.
 */
s16 world_formation_build_record_list(s32 item_id, world_unit_t** unused_list, s32 mode) {
    unit_stats_t unit;
    s32 slot = 0;
    s32 count = 0;
    s32 id = item_id & ITEM_ENTRY_ID_MASK;
    s32 found;
    s32 i;

    for (; slot < PARTY_ROSTER_SLOT_COUNT; slot++) {
        if (main_unit_init_job_data(&unit, slot, 1) != 0) {
            continue;
        }
        world_formation_build_unit_record(&unit, &g_world_unit_records[count], main_party_get_unit(slot));
        if (mode == WORLD_FORMATION_RECORD_LIST_AVAILABLE_HUMANS) {
            if (g_world_unit_records[count].proposition_status != 0) {
                continue;
            }
            if (g_world_unit_records[count].gender_flags & UNIT_FLAG_EGG) {
                continue;
            }
            if (g_world_unit_records[count].uses_monster_skillset != 0) {
                continue;
            }
            if (job_is_special_monster(g_world_unit_records[count].identity.job_id) != 0) {
                continue;
            }
        } else if (mode == WORLD_FORMATION_RECORD_LIST_MONSTERS) {
            if (g_world_unit_records[count].gender_flags & UNIT_FLAG_EGG) {
                continue;
            }
            if ((g_world_unit_records[count].gender_flags >> 6) != 0) {
                continue;
            }
        }
        if (id > 0) {
            found = 0;
            for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                if (g_world_unit_records[count].equipment[i] == id) {
                    found = 1;
                }
            }
            if (found == 0) {
                continue;
            }
        }
        g_world_formation_record_order[count] = count;
        count++;
    }
    g_world_formation_record_count = count;
    world_formation_sort_roster(0);
    return g_world_formation_record_count;
}

/*
 * Write each formation record's job, abilities, and equipment back into its
 * roster party_unit_t, then rebuild the record from the refreshed unit.
 *
 * Records using monster skillsets keep their party equipment, learned
 * abilities, and JP. Shields go to the hand's shield byte.
 */
void world_formation_save_records_to_party(void) {
    unit_stats_t unit;
    party_unit_t* party;
    s32 i;
    s16 slot;
    u32 item;
    u8 value;

    for (i = 0; i < g_world_formation_record_count; i++) {
        slot = g_world_unit_pointers[i]->identity.roster_slot;
        party = main_party_get_unit(slot);
        party->job_id = g_world_unit_pointers[i]->identity.job_id;
        party->gender_flags = g_world_unit_pointers[i]->gender_flags;
        party->secondary_skillset = g_world_unit_pointers[i]->abilities.fields.secondary_skillset;
        party->reaction_ability[0] = g_world_unit_pointers[i]->abilities.fields.reaction_ability;
        party->reaction_ability[1] = g_world_unit_pointers[i]->abilities.fields.reaction_ability >> 8;
        party->support_ability[0] = g_world_unit_pointers[i]->abilities.fields.support_ability;
        party->support_ability[1] = g_world_unit_pointers[i]->abilities.fields.support_ability >> 8;
        party->movement_ability[0] = g_world_unit_pointers[i]->abilities.fields.movement_ability;
        party->movement_ability[1] = g_world_unit_pointers[i]->abilities.fields.movement_ability >> 8;
        if (g_world_unit_pointers[i]->uses_monster_skillset == 0) {
            item = g_world_unit_pointers[i]->equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND];
            party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
            party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = ITEM_ID_NONE;
            if (item != ITEM_ID_NOTHING) {
                if (g_main_item_primary_data[item].type == ITEM_TYPE_SHIELD) {
                    party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = item;
                } else {
                    party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = item;
                }
            }
            item = g_world_unit_pointers[i]->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND];
            party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
            party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = ITEM_ID_NONE;
            if (item != ITEM_ID_NOTHING) {
                if (g_main_item_primary_data[item].type == ITEM_TYPE_SHIELD) {
                    party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = item;
                } else {
                    party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = item;
                }
            }
            value = ITEM_ID_NONE;
            if (g_world_unit_pointers[i]->equipment[MENU_EQUIPMENT_SLOT_HEAD] != ITEM_ID_NOTHING) {
                value = g_world_unit_pointers[i]->equipment[MENU_EQUIPMENT_SLOT_HEAD];
            }
            party->equipment[UNIT_EQUIPMENT_SLOT_HEAD] = value;
            value = ITEM_ID_NONE;
            if (g_world_unit_pointers[i]->equipment[MENU_EQUIPMENT_SLOT_BODY] != ITEM_ID_NOTHING) {
                value = g_world_unit_pointers[i]->equipment[MENU_EQUIPMENT_SLOT_BODY];
            }
            party->equipment[UNIT_EQUIPMENT_SLOT_BODY] = value;
            value = ITEM_ID_NONE;
            if (g_world_unit_pointers[i]->equipment[MENU_EQUIPMENT_SLOT_ACCESSORY] != ITEM_ID_NOTHING) {
                value = g_world_unit_pointers[i]->equipment[MENU_EQUIPMENT_SLOT_ACCESSORY];
            }
            party->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY] = value;
            bcopy(g_world_unit_pointers[i]->learned_abilities, party->career.learned_abilities,
                sizeof(party->career.learned_abilities));
            bcopy(&g_world_unit_pointers[i]->job_points.current, &party->career.job_points.current,
                sizeof(party->career.job_points.current));
            bcopy(&g_world_unit_pointers[i]->job_points.total, &party->career.job_points.total,
                sizeof(party->career.job_points.total));
        }
        main_unit_init_job_data(&unit, slot, 1);
        world_formation_build_unit_record(&unit, g_world_unit_pointers[i], party);
    }
}
