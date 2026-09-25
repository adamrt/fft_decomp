#include "fft/battle.h"
#include "fft/bunit.h"
#include "fft/main_unit.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Builds a BUNIT unit-data record from a battle unit: clamps level, HP/MP
 * and CT for display, lists up to four monster-skillset abilities (the fourth
 * only when battle_menu_init_monster_skill_check passes) or the equipment and
 * ability slots, and copies the job and ability tables with JP capped at 9999.
 *
 * BUNIT twin of world_formation_build_unit_record. `count` indexes the output
 * slot so loop.c strength-reduces it into the record pointer; with an explicit
 * pointer walk the smaller loop lets loop.c hoist the constant 3 into $s6. The
 * caller also passes its shown-list slot, which is not read. */
void bunit_panel_copy_unit_data_to_billboard(battle_stats_t* unit, bunit_unit_data_t* record, s32 unused_slot) {
    s16* abilities;
    s32 count;
    s32 i;
    s32 found;
    s32 value;
    s32 list_index;
    u32 evade;
    u32 other;

    memset(record, 0, sizeof(bunit_unit_data_t));
    record->level = unit->level < 100 ? unit->level : 99;
    if (unit->initial_team_flags & BATTLE_TEAM_MASK) {
        record->team_kind = 1;
    }
    if (!(unit->initial_team_flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK)) {
        record->team_kind = 2;
    }
    if (unit->auto_battle_setting != 0) {
        record->team_kind = 3;
    }
    i = battle_action_check_at_list_for_unit_battle_id(unit);
    list_index = -1;
    if (i >= 0) {
        list_index = i + 1;
    }
    record->list_index = list_index;
    record->experience = unit->experience < 100 ? unit->experience : 99;
    record->entd_slot = unit->entd_slot;
    record->current_hp = unit->hp < 1000 ? unit->hp : 999;
    record->max_hp = unit->max_hp < 1000 ? unit->max_hp : 999;
    record->current_mp = unit->mp < 1000 ? unit->mp : 999;
    record->max_mp = unit->max_mp < 1000 ? unit->max_mp : 999;
    value = 100;
    if (unit->has_turn != 1) {
        value = unit->ct;
    }
    record->current_ct = value;
    if (value >= 101) {
        record->current_ct = 100;
    }
    record->max_ct = 100;
    record->formation_index = unit->formation_index;
    record->entd_slot_22 = unit->entd_slot;
    record->monster_base_job_id = unit->job_id;
    record->brave = unit->brave;
    record->faith = unit->faith;
    record->zodiac = *(u16*)&unit->birthday >> 12;
    record->roster_id = unit->entd_slot;
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
    other = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    if (evade < other) {
        evade = other;
    }
    record->physical_shield_evade = evade;
    record->physical_accessory_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE];
    record->magical_attack = unit->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    record->magical_class_evade = 0;
    evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    other = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    if (evade < other) {
        evade = other;
    }
    record->magical_shield_evade = evade;
    record->magical_accessory_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];

    if (record->uses_monster_skillset != 0) {
        abilities = (s16*)main_ability_store_skillset_abilities(unit->primary_skillset, SKILLSET_ABILITY_FILTER_ACTION);
        count = 0;
        for (i = 0; i < 4; i++) {
            found = 0;
            if (i == 3) {
                if (abilities[3] != 0) {
                    found = battle_menu_init_monster_skill_check(unit) != 0;
                }
            } else if (abilities[i] != 0) {
                found = 1;
            }
            if (found) {
                record->abilities[count++] = abilities[i];
            }
        }
    } else {
        record->equipment[0] = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE
            ? unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]
            : unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
        record->equipment[1] = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE
            ? unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]
            : unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
        record->equipment[2] = unit->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
        record->equipment[3] = unit->equipment[UNIT_EQUIPMENT_SLOT_BODY];
        record->equipment[4] = unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
        for (i = 0; i < 5; i++) {
            if (record->equipment[i] >= 0xfe) {
                record->equipment[i] = ITEM_ID_NOTHING;
            }
        }
        record->abilities[0] = unit->primary_skillset;
        record->abilities[1] = unit->secondary_skillset;
        record->abilities[2] = unit->reaction_ability;
        record->abilities[3] = unit->support_ability;
        record->abilities[4] = unit->movement_ability;
    }

    record->misc_unit_id = battle_unit_get_misc_id_by_battle_id(unit->entd_slot);
    record->initial_team_flags = unit->initial_team_flags;
    record->formation_order_key = unit->mount_info;
    record->two_handed = bunit_unit_is_two_handing_weapon(record->equipment, unit->support_abilities[2] & 2);
    record->gender_flags = unit->unit_flags;
    record->character_identity = unit->character_identity;
    if (bunit_job_is_special_monster(unit->job_id) != 0 || record->uses_monster_skillset != 0
        || record->team_kind == 1) {
        bcopy(unit->unlocked_jobs, record->unlocked_jobs, sizeof(record->unlocked_jobs));
    } else {
        i = main_job_calculate_unlocked(unit->job_levels, unit->unit_flags & UNIT_FLAG_MALE);
        record->unlocked_jobs[0] = i >> 16;
        record->unlocked_jobs[1] = i >> 8;
        record->unlocked_jobs[2] = i;
    }
    bcopy(unit->learned_abilities, record->learned_abilities, sizeof(record->learned_abilities));
    bcopy(unit->job_levels, record->job_levels, sizeof(record->job_levels));
    bcopy(unit->job_points, record->job_points, sizeof(record->job_points));
    bcopy(unit->total_job_points, record->total_job_points, sizeof(record->total_job_points));
    for (i = 0; i < 20; i++) {
        if (record->job_points[i] >= 10000) {
            record->job_points[i] = 9999;
        }
        if (record->total_job_points[i] >= 10000) {
            record->total_job_points[i] = 9999;
        }
    }
}
