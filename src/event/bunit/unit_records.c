/* Unit records built from stats, the data list, sorting by order keys and copying between order lists. */

#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "fft/data_item.h"
#include "fft/data_skillset.h"
#include "fft/data_status.h"
#include "fft/event_bunit.h"
#include "fft/job.h"
#include "fft/main_item.h"
#include "fft/main_unit.h"
#include "fft/menu_status.h"
#include "fft/unit.h"
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
static void bunit_unit_build_record_from_stats(const unit_stats_t* unit, bunit_unit_t* record, s32 unused_slot) {
    s16* abilities;
    s32 count;
    s32 i;
    s32 found;
    s32 value;
    s32 list_index;
    u32 evade;
    u32 other;

    memset(record, 0, sizeof(bunit_unit_t));
    record->level = unit->level <= UNIT_LEVEL_MAX ? unit->level : UNIT_LEVEL_MAX;

    /* Later checks win: 1 enemy team, 2 neither enemy nor player controlled, 3 auto battle. */
    if (unit->initial_team_flags & UNIT_TEAM_MASK) {
        record->team_kind = 1;
    }
    if (!(unit->initial_team_flags & UNIT_TEAM_OR_PLAYER_CONTROL_MASK)) {
        record->team_kind = 2;
    }
    if (unit->auto_battle_setting != AI_AUTOBATTLE_MANUAL) {
        record->team_kind = 3;
    }

    /* 1-based place in the AT (turn order) list, or -1 when the unit is not listed. */
    i = battle_action_find_unit_turn_index(unit);
    list_index = -1;
    if (i >= 0) {
        list_index = i + 1;
    }
    record->list_index = list_index;

    record->experience = unit->experience < UNIT_LEVEL_UP_EXPERIENCE ? unit->experience : UNIT_LEVEL_EXPERIENCE_MAX;
    record->entd_slot = unit->entd_slot;
    record->gauges[MENU_UNIT_STATUS_GAUGE_HP].value = unit->hp <= UNIT_HP_MP_MAX ? unit->hp : UNIT_HP_MP_MAX;
    record->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = unit->max_hp <= UNIT_HP_MP_MAX ? unit->max_hp : UNIT_HP_MP_MAX;
    record->gauges[MENU_UNIT_STATUS_GAUGE_MP].value = unit->mp <= UNIT_HP_MP_MAX ? unit->mp : UNIT_HP_MP_MAX;
    record->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = unit->max_mp <= UNIT_HP_MP_MAX ? unit->max_mp : UNIT_HP_MP_MAX;

    /* A unit whose turn it is shows a full CT gauge. */
    value = UNIT_TURN_CT_THRESHOLD;
    if (unit->has_turn != 1) {
        value = unit->ct;
    }
    record->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = value;
    if (value > UNIT_TURN_CT_THRESHOLD) {
        record->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = UNIT_TURN_CT_THRESHOLD;
    }
    record->gauges[MENU_UNIT_STATUS_GAUGE_CT].max = UNIT_TURN_CT_THRESHOLD;

    record->formation_index = unit->formation_index;
    /* The ENTD slot also serves as the record's name index and battle id. */
    record->name_index = unit->entd_slot;
    record->job_id = unit->job_id;
    record->brave = unit->brave;
    record->faith = unit->faith;
    record->zodiac = unit->birthday.zodiac;
    record->battle_id = unit->entd_slot;

    record->move = unit->move;
    record->speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    record->jump = unit->jump;
    record->right_weapon_power = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    record->left_weapon_power = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];
    record->right_weapon_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    record->left_weapon_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    i = unit->primary_skillset >= SKILLSET_ID_MONSTER_FIRST && unit->primary_skillset < SKILLSET_ID_END;
    record->uses_monster_skillset = i;

    record->physical_attack = unit->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    record->physical_class_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    other = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    if (evade < other) {
        evade = other;
    }
    record->physical_shield_evade = evade;
    record->physical_accessory_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE];

    record->magical_attack = unit->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    record->magical_class_evade = 0;
    evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    other = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    if (evade < other) {
        evade = other;
    }
    record->magical_shield_evade = evade;
    record->magical_accessory_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];

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
            if (record->equipment[i] >= ITEM_ID_END) {
                record->equipment[i] = ITEM_ID_NOTHING;
            }
        }

        record->abilities[0] = unit->primary_skillset;
        record->abilities[1] = unit->secondary_skillset;
        record->abilities[2] = unit->abilities.reaction;
        record->abilities[3] = unit->abilities.support;
        record->abilities[4] = unit->abilities.movement;
    }

    record->runtime_id = battle_unit_get_runtime_id_by_battle_id(unit->entd_slot);
    record->initial_team_flags = unit->initial_team_flags;
    record->mount_info = unit->mount_info;
    record->two_handed = unit_is_two_handing_weapon(
        record->equipment, unit->ability_flags.support[2] & BATTLE_SUPPORT_SET_3_TWO_HANDS);
    record->gender_flags = unit->unit_flags;
    record->character_identity = unit->character_identity;

    if (job_is_special_monster(unit->job_id) != 0 || record->uses_monster_skillset != 0 || record->team_kind == 1) {
        bcopy(unit->career.unlocked_jobs, record->unlocked_jobs, sizeof(record->unlocked_jobs));
    } else {
        i = main_job_calculate_unlocked(unit->career.job_levels, unit->unit_flags & UNIT_FLAG_MALE);
        /* The 24-bit unlocked-job mask is stored high byte first. */
        record->unlocked_jobs[0] = i >> 16;
        record->unlocked_jobs[1] = i >> 8;
        record->unlocked_jobs[2] = i;
    }

    bcopy(unit->career.learned_abilities, record->learned_abilities, sizeof(record->learned_abilities));
    bcopy(unit->career.job_levels, record->job_levels, sizeof(record->job_levels));
    bcopy(&unit->career.job_points.current, &record->job_points.current, sizeof(record->job_points.current));
    bcopy(&unit->career.job_points.total, &record->job_points.total, sizeof(record->job_points.total));

    for (i = 0; i < UNIT_CAREER_JOB_COUNT; i++) {
        if (record->job_points.current.values[i] > UNIT_JOB_POINT_MAX) {
            record->job_points.current.values[i] = UNIT_JOB_POINT_MAX;
        }
        if (record->job_points.total.values[i] > UNIT_JOB_POINT_MAX) {
            record->job_points.total.values[i] = UNIT_JOB_POINT_MAX;
        }
    }
}

/* Copy every battle unit that is not an egg, crystal or treasure into the next
 * status billboard record, store the shown-plus-egg unit count in each
 * record and sort the shown list; returns the number of shown units. `mode`
 * and `unit_data` are not read. */
s32 bunit_unit_build_data_list(s32 mode, s32 unit_data, s32 sort_mode) {
    unit_stats_t* unit;
    s32 i;
    s32 shown = 0;
    s32 total = 0;

    /* Keep status-byte reads separate; GCC otherwise merges the two tests into one 16-bit load. */
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = battle_unit_get_stats_from_battle_id(i);
        if ((unit != 0) && (unit->entd_slot != BATTLE_ENTD_SLOT_NONE)) {
            if (unit->unit_flags & UNIT_FLAG_EGG) {
                total++;
            } else if (!(*&unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                           & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
                && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
                    & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
                total++;
                bunit_unit_build_record_from_stats(unit, &g_bunit_unit_records[shown], shown);
                g_bunit_shown_unit_indices[shown] = shown;
                shown++;
            }
        }
    }

    for (i = 0; i < shown; i++) {
        g_bunit_unit_records[i].unit_count = total;
    }
    g_bunit_unit_count = shown;
    bunit_unit_sort_index_list_by_order_keys(sort_mode);
    return shown;
}

/* Bubble-sort g_bunit_shown_unit_indices (the shown-unit index list) by the
 * successive keys of g_main_item_type_order_tables.order_0 (the unit sort order the
 * reorder list edits, each key a record field picked through g_bunit_unit_sort_field_indices; a tie
 * moves on to the next key), then publish the sorted record pointers into
 * g_bunit_unit_pointers. Keys 1 and 4 and up sort in descending order. `sort_mode` is
 * not read. BUNIT twin of world_formation_sort_roster. */
void bunit_unit_sort_index_list_by_order_keys(s32 sort_mode) {
    s32 count;
    s32 j;
    s32 k;
    s32 diff;
    s32 lhs;
    s32 rhs;
    s16* rec; /* Native sort keys select signed 16-bit values within the record. */

    for (count = g_bunit_unit_count - 1; count > 0; count--) {
        for (j = 0; j < count; j++) {
            k = 0;
            do {
                rec = (s16*)&g_bunit_unit_records[g_bunit_shown_unit_indices[j]];
                lhs = rec[g_bunit_unit_sort_field_indices[g_main_item_type_order_tables.order_0[k]]];
                rec = (s16*)&g_bunit_unit_records[g_bunit_shown_unit_indices[j + 1]];
                rhs = rec[g_bunit_unit_sort_field_indices[g_main_item_type_order_tables.order_0[k]]];
                diff = rhs - lhs;
                k++;
            } while (diff == 0);
            if (g_main_item_type_order_tables.order_0[k - 1] == 1
                || g_main_item_type_order_tables.order_0[k - 1] >= 4) {
                diff = -diff;
            }
            if (diff < 0) {
                s32 swap = g_bunit_shown_unit_indices[j];
                g_bunit_shown_unit_indices[j] = g_bunit_shown_unit_indices[j + 1];
                g_bunit_shown_unit_indices[j + 1] = swap;
            }
        }
    }
    for (j = 0; j < g_bunit_unit_count; j++) {
        g_bunit_unit_pointers[j] = &g_bunit_unit_records[g_bunit_shown_unit_indices[j]];
    }
}

/* Copies the first signed-byte order list into a -1-terminated s16 list. */
void bunit_unit_copy_orders_to_reorder_list(s32 unused, s16* entries) {
    s32 i;

    for (i = 0; (*entries = g_main_item_type_order_tables.order_0[i]) != -1; i++) {
        entries++;
    }
}

/* Copy the low byte of each 16-bit entry into g_main_item_type_order_tables.order_0
 * (the first, 12-byte list of those tables) until the terminator (-1) has been copied.
 * The first parameter is unused. */
void bunit_unit_copy_reorder_list_to_orders(s32 unused, const s16* entries) {
    s32 index = 0;
    s8 value = *entries;

    g_main_item_type_order_tables.order_0[index] = value;
    while (value != -1) {
        entries++;
        value = *entries;
        index += 1;
        g_main_item_type_order_tables.order_0[index] = value;
    }
}

/* Find the BUNIT record with a battle id, or return the active record count.
 *
 * The not-found result doubles as the next/end index for both known callers. */
s32 bunit_unit_find_index_by_battle_id(s16 battle_id) {
    s32 stack_scratch[2]; /* Unused; the target keeps an 8-byte leaf frame. */
    s32 count;
    s32 i;
    s32 search_value;
    bunit_unit_t** units;

    count = g_bunit_unit_count;
    i = 0;
    if (count > 0) {
        search_value = battle_id;
        units = g_bunit_unit_pointers;
        do {
            if ((*units)->battle_id == search_value) {
                return i;
            }
            i++;
            units++;
        } while (i < count);
    }
    return i;
}
