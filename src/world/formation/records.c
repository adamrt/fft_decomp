/* The Formation workspace presents party records to the menus and writes edited jobs, abilities and equipment
 * back to the roster. Its extra record holds job and equipment previews without committing them. */

#include "fft/ability.h"
#include "fft/formation.h"
#include "fft/formation_ability.h"
#include "fft/item.h"
#include "fft/item_menu.h"
#include "fft/job.h"
#include "fft/menu.h"
#include "fft/party.h"
#include "fft/skillset.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "fft/unit_party.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Sorting binds the final record to the shared preview pointer. */
formation_unit_t g_formation_unit_records[FORMATION_UNIT_RECORD_COUNT] = { 0 };

/*
 * Fills one WORLD formation record from a unit's battle stats and party data.
 *
 * HP/MP/level/experience/CT are clamped for display, monster-skillset units
 * (primary skillset 0xb0-0xdf) list up to three nonzero skillset abilities
 * instead of equipment and learned abilities, a Mime shows no equipment or
 * set abilities, and JP totals cap at 9999. A hand slot shows its weapon, or
 * its shield when it holds none; the larger of the two shields' evades is
 * shown.
 * One scratch variable `i` also carries the monster flag, the second evade
 * operand and the unlocked-job mask; separate locals reallocate a0/v1.
 */
void formation_unit_record_build(const unit_stats_t* unit, formation_unit_t* record, const party_unit_t* party) {
    s16* abilities;
    s32 i;
    u32 evade;

    memset(record, 0, sizeof(formation_unit_t));
    record->status.level = party->level <= UNIT_LEVEL_MAX ? party->level : UNIT_LEVEL_MAX;
    if (unit->formation_index >= PARTY_GUEST_SLOT_FIRST) {
        record->status.team_kind = 2; /* guests show the neutral team icon */
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
    record->detail.move = unit->move;
    record->detail.speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    record->detail.jump = unit->jump;
    record->detail.right_weapon_power = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    record->detail.left_weapon_power = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];
    record->detail.right_weapon_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    record->detail.left_weapon_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    i = unit->primary_skillset >= SKILLSET_ID_MONSTER_FIRST && unit->primary_skillset < SKILLSET_ID_END;
    record->detail.uses_monster_skillset = i;
    record->detail.physical_attack = unit->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    record->detail.physical_class_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    i = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    if (evade < i) {
        evade = i;
    }
    record->detail.physical_shield_evade = evade;
    record->detail.physical_accessory_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE];
    record->detail.magical_attack = unit->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    record->detail.magical_class_evade = 0;
    evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    i = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    if (evade < i) {
        evade = i;
    }
    record->detail.magical_shield_evade = evade;
    record->detail.magical_accessory_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];

    /* The record's equipment and abilities. */
    if (record->detail.uses_monster_skillset != 0) {
        s32 count;

        /* The signed view keeps the original single-load copy loop. */
        abilities = (s16*)ability_skillset_list_build(unit->primary_skillset, SKILLSET_ABILITY_FILTER_ACTION);
        count = 0;
        for (i = 0; i < 3; i++) {
            if (abilities[i] != 0) {
                record->detail.abilities[count++] = abilities[i];
            }
        }
    } else {
        record->detail.abilities[FORMATION_ABILITY_SLOT_PRIMARY_SKILLSET] = unit->primary_skillset;
        if (record->identity.job_id != JOB_ID_MIME) {
            record->detail.equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND]
                = party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE
                ? party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]
                : party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
            record->detail.equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND]
                = party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE
                ? party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]
                : party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
            record->detail.equipment[MENU_EQUIPMENT_SLOT_HEAD] = party->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
            record->detail.equipment[MENU_EQUIPMENT_SLOT_BODY] = party->equipment[UNIT_EQUIPMENT_SLOT_BODY];
            record->detail.equipment[MENU_EQUIPMENT_SLOT_ACCESSORY] = party->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
            for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                if (record->detail.equipment[i] >= ITEM_ID_END) {
                    record->detail.equipment[i] = ITEM_ID_NOTHING;
                }
            }
            record->detail.abilities[FORMATION_ABILITY_SLOT_SECONDARY_SKILLSET] = party->secondary_skillset;
            record->detail.abilities[FORMATION_ABILITY_SLOT_REACTION]
                = party->reaction_ability.low + (party->reaction_ability.high << 8);
            record->detail.abilities[FORMATION_ABILITY_SLOT_SUPPORT]
                = party->support_ability.low + (party->support_ability.high << 8);
            record->detail.abilities[FORMATION_ABILITY_SLOT_MOVEMENT]
                = party->movement_ability.low + (party->movement_ability.high << 8);
        }
    }

    /* Monsters keep their stored unlocked-job bits; humans recompute theirs from the job levels. */
    if (job_is_special_monster(unit->job_id) != 0 || record->detail.uses_monster_skillset != 0) {
        bcopy(unit->career.unlocked_jobs, record->unlocked_jobs, sizeof(record->unlocked_jobs));
    } else {
        i = job_unlocks_calculate(unit->career.job_levels, unit->unit_flags & (UNIT_FLAG_FEMALE | UNIT_FLAG_MALE));
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
    record->detail.two_handed = unit_is_two_handing_weapon(
        (s16*)record->detail.equipment, record->support_abilities[2] & UNIT_SUPPORT_SET_3_TWO_HANDS);
}

/*
 * Rebuilds the compacted WORLD formation records from the 20 roster slots.
 *
 * AVAILABLE_HUMANS drops units away on a proposition, eggs, units with a
 * monster skillset and special-monster jobs; MONSTERS drops eggs and humans.
 * A nonzero item id keeps only units with that item equipped. Returns the
 * record count. The list argument is unused here;
 * formation_roster_sort fills g_formation_unit_pointers.
 */
s16 formation_record_list_build(s32 item_id, formation_unit_t** unused_list, formation_record_list_mode_e mode) {
    unit_stats_t unit;
    s32 slot = 0;
    s32 count = 0;
    s32 id = item_id & ITEM_ENTRY_ID_MASK;
    s32 found;
    s32 i;

    for (; slot < PARTY_ROSTER_SLOT_COUNT; slot++) {
        if (unit_party_slot_init(&unit, slot, 1) != 0) {
            continue;
        }
        formation_unit_record_build(&unit, &g_formation_unit_records[count], party_unit_get(slot));
        if (mode == FORMATION_RECORD_LIST_AVAILABLE_HUMANS) {
            if (g_formation_unit_records[count].proposition_status != 0) {
                continue;
            }
            if (g_formation_unit_records[count].gender_flags & UNIT_FLAG_EGG) {
                continue;
            }
            if (g_formation_unit_records[count].detail.uses_monster_skillset != 0) {
                continue;
            }
            if (job_is_special_monster(g_formation_unit_records[count].identity.job_id) != 0) {
                continue;
            }
        } else if (mode == FORMATION_RECORD_LIST_MONSTERS) {
            if (g_formation_unit_records[count].gender_flags & UNIT_FLAG_EGG) {
                continue;
            }
            /* The male and female bits, 0x80 and 0x40. */
            if ((g_formation_unit_records[count].gender_flags >> 6) != 0) {
                continue;
            }
        }
        if (id > 0) {
            found = 0;
            for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                if (g_formation_unit_records[count].detail.equipment[i] == id) {
                    found = 1;
                }
            }
            if (found == 0) {
                continue;
            }
        }
        g_formation_record_order[count] = count;
        count++;
    }
    g_formation_record_count = count;
    formation_roster_sort(0);
    return g_formation_record_count;
}

/*
 * Write each formation record's job, abilities, and equipment back into its
 * roster party_unit_t, then rebuild the record from the refreshed unit.
 *
 * Records using monster skillsets keep their party equipment, learned
 * abilities, and JP. Shields go to the hand's shield byte.
 */
void formation_save_records(void) {
    unit_stats_t unit;
    party_unit_t* party;
    s32 i;
    s16 slot;
    u32 item;
    u8 value;

    for (i = 0; i < g_formation_record_count; i++) {
        slot = g_formation_unit_pointers[i]->identity.roster_slot;
        party = party_unit_get(slot);
        party->job_id = g_formation_unit_pointers[i]->identity.job_id;
        party->gender_flags = g_formation_unit_pointers[i]->gender_flags;
        party->secondary_skillset
            = g_formation_unit_pointers[i]->detail.abilities[FORMATION_ABILITY_SLOT_SECONDARY_SKILLSET];
        party->reaction_ability.low = g_formation_unit_pointers[i]->detail.abilities[FORMATION_ABILITY_SLOT_REACTION];
        party->reaction_ability.high
            = g_formation_unit_pointers[i]->detail.abilities[FORMATION_ABILITY_SLOT_REACTION] >> 8;
        party->support_ability.low = g_formation_unit_pointers[i]->detail.abilities[FORMATION_ABILITY_SLOT_SUPPORT];
        party->support_ability.high
            = g_formation_unit_pointers[i]->detail.abilities[FORMATION_ABILITY_SLOT_SUPPORT] >> 8;
        party->movement_ability.low = g_formation_unit_pointers[i]->detail.abilities[FORMATION_ABILITY_SLOT_MOVEMENT];
        party->movement_ability.high
            = g_formation_unit_pointers[i]->detail.abilities[FORMATION_ABILITY_SLOT_MOVEMENT] >> 8;
        if (g_formation_unit_pointers[i]->detail.uses_monster_skillset == 0) {
            item = g_formation_unit_pointers[i]->detail.equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND];
            party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
            party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = ITEM_ID_NONE;
            if (item != ITEM_ID_NOTHING) {
                if (g_item_primary_data[item].type == ITEM_TYPE_SHIELD) {
                    party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = item;
                } else {
                    party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = item;
                }
            }
            item = g_formation_unit_pointers[i]->detail.equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND];
            party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
            party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = ITEM_ID_NONE;
            if (item != ITEM_ID_NOTHING) {
                if (g_item_primary_data[item].type == ITEM_TYPE_SHIELD) {
                    party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = item;
                } else {
                    party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = item;
                }
            }
            value = ITEM_ID_NONE;
            if (g_formation_unit_pointers[i]->detail.equipment[MENU_EQUIPMENT_SLOT_HEAD] != ITEM_ID_NOTHING) {
                value = g_formation_unit_pointers[i]->detail.equipment[MENU_EQUIPMENT_SLOT_HEAD];
            }
            party->equipment[UNIT_EQUIPMENT_SLOT_HEAD] = value;
            value = ITEM_ID_NONE;
            if (g_formation_unit_pointers[i]->detail.equipment[MENU_EQUIPMENT_SLOT_BODY] != ITEM_ID_NOTHING) {
                value = g_formation_unit_pointers[i]->detail.equipment[MENU_EQUIPMENT_SLOT_BODY];
            }
            party->equipment[UNIT_EQUIPMENT_SLOT_BODY] = value;
            value = ITEM_ID_NONE;
            if (g_formation_unit_pointers[i]->detail.equipment[MENU_EQUIPMENT_SLOT_ACCESSORY] != ITEM_ID_NOTHING) {
                value = g_formation_unit_pointers[i]->detail.equipment[MENU_EQUIPMENT_SLOT_ACCESSORY];
            }
            party->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY] = value;
            bcopy(g_formation_unit_pointers[i]->learned_abilities, party->career.learned_abilities,
                sizeof(party->career.learned_abilities));
            bcopy(&g_formation_unit_pointers[i]->job_points.current, &party->career.job_points.current,
                sizeof(party->career.job_points.current));
            bcopy(&g_formation_unit_pointers[i]->job_points.total, &party->career.job_points.total,
                sizeof(party->career.job_points.total));
        }
        unit_party_slot_init(&unit, slot, 1);
        formation_unit_record_build(&unit, g_formation_unit_pointers[i], party);
    }
}
