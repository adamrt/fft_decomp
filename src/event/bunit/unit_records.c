/* Unit List scrolling checks, unit records and their sort order. */

#include "fft/ability.h"
#include "fft/at_list.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/menu.h"
#include "fft/skillset.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "fft/unit_list.h"
#include "fft/unit_runtime.h"
#include "psx/libc.h"
#include "psx/types.h"

static s16 g_unit_list_shown_unit_indices[22] = { 0 };
static s16 g_unit_list_count = 0;
static unit_list_record_t g_unit_list_records[21] = { 0 };

/* Returns 1 while the unit list is scrolling, or when the selected unit lies
 * outside the three rows of four shown at the current page offset, so that
 * the list has to scroll. */
s32 unit_list_is_scroll_needed(void) {
    s32 ret = 0;

    if (g_unit_list_scroll_step == 0) {
        switch (g_unit_list_page_offset) {
        case -0x78:
            if (g_unit_selected_index < 8) {
                ret = 1;
            }
            break;
        case -0x3C:
            if (g_unit_selected_index >= 0x10) {
                ret = 1;
            } else if (g_unit_selected_index < 4) {
                ret = 1;
            }
            break;
        case 0:
            if (g_unit_selected_index >= 0xC) {
                ret = 1;
            }
            break;
        }
    } else {
        ret = 1;
    }
    return ret;
}

/* Returns 1 when the unit holds a single weapon in two hands: exactly one of
 * the two hand slots is occupied, its item is below the throwable range, and
 * the weapon either forces two hands or allows them while `two_hands_support`
 * (the Two Hands support ability) is set. */
s32 unit_is_two_handing_weapon(const s16* slots, s32 two_hands_support) {
    s32 result = 0;
    s32 item_id = 0;

    if (slots[0] != 0) {
        if (slots[1] == 0) {
            item_id = slots[0] & 0x3FF;
        }
    } else if (slots[1] != 0) {
        item_id = slots[1] & 0x3FF;
    }
    if (item_id != 0 && item_id < ITEM_ID_THROWABLE_FIRST) {
        u8 flags = g_item_weapon_data[item_id].flags;
        if (flags & WEAPON_FLAG_FORCED_TWO_HANDS) {
            result = 1;
        } else if (two_hands_support != 0 && (flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE)) {
            result = 1;
        }
    }
    return result;
}

/* Returns 1 for a job id in either special-monster range: from
 * JOB_ID_SPECIAL_MONSTER_1_FIRST up to the first generic job, or the
 * block that starts at JOB_ID_SPECIAL_MONSTER_2_FIRST. */
s32 job_is_special_monster(s32 job_id) {
    s32 result = job_id >= JOB_ID_SPECIAL_MONSTER_1_FIRST && job_id < JOB_ID_GENERIC_FIRST;

    if (job_id >= JOB_ID_SPECIAL_MONSTER_2_FIRST
        && job_id < JOB_ID_SPECIAL_MONSTER_2_FIRST + JOB_ID_SPECIAL_MONSTER_2_COUNT) {
        result = 1;
    }
    return result;
}

/* Build a Unit List record from a battle unit: clamps level, HP/MP
 * and CT for display, lists up to four monster-skillset abilities (the fourth
 * only when skillset_monster_count_supporters passes) or the equipment and
 * ability slots, and copies the job and ability tables with JP capped at 9999.
 *
 * `count` indexes the output
 * slot so loop.c strength-reduces it into the record pointer; with an explicit
 * pointer walk the smaller loop lets loop.c hoist the constant 3 into $s6. The
 * caller also passes its shown-list slot, which is not read. */
static void unit_list_build_record(const unit_stats_t* unit, unit_list_record_t* record, s32 unused_slot) {
    s16* abilities;
    s32 count;
    s32 i;
    s32 found;
    s32 value;
    s32 list_index;
    u32 evade;
    u32 other;

    memset(record, 0, sizeof(unit_list_record_t));
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
    i = at_list_find_unit(unit);
    list_index = -1;
    if (i >= 0) {
        list_index = i + 1;
    }
    record->list_index = list_index;

    record->experience = unit->experience < UNIT_LEVEL_UP_EXPERIENCE ? unit->experience : UNIT_LEVEL_EXPERIENCE_MAX;
    record->active_battle_id = unit->active_battle_id;
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
    record->name_index = unit->active_battle_id;
    record->job_id = unit->job_id;
    record->brave = unit->brave;
    record->faith = unit->faith;
    record->zodiac = unit->birthday.zodiac;
    record->battle_id = unit->active_battle_id;

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
        abilities = (s16*)ability_store_skillset_abilities(unit->primary_skillset, SKILLSET_ABILITY_FILTER_ACTION);
        count = 0;
        for (i = 0; i < 4; i++) {
            found = 0;
            if (i == 3) {
                if (abilities[3] != 0) {
                    found = skillset_monster_count_supporters(unit) != 0;
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

    record->runtime_id = unit_get_runtime_id_by_battle_id(unit->active_battle_id);
    record->initial_team_flags = unit->initial_team_flags;
    record->mount_info = unit->mount_info;
    record->two_handed
        = unit_is_two_handing_weapon(record->equipment, unit->ability_flags.support[2] & UNIT_SUPPORT_SET_3_TWO_HANDS);
    record->gender_flags = unit->unit_flags;
    record->character_identity = unit->character_identity;

    if (job_is_special_monster(unit->job_id) != 0 || record->uses_monster_skillset != 0 || record->team_kind == 1) {
        bcopy(unit->career.unlocked_jobs, record->unlocked_jobs, sizeof(record->unlocked_jobs));
    } else {
        i = job_calculate_unlocked(unit->career.job_levels, unit->unit_flags & UNIT_FLAG_MALE);
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
 * Unit List record, store the shown-plus-egg unit count in each record and
 * sort the shown list. Returns the number of shown units; the first two
 * parameters are retained but not read. */
s32 unit_list_build_records(s32 unused_mode, unit_list_record_t** unused_records, s32 sort_mode) {
    unit_stats_t* unit;
    s32 i;
    s32 shown = 0;
    s32 total = 0;

    /* Keep status-byte reads separate; GCC otherwise merges the two tests into one 16-bit load. */
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = unit_get_stats(i);
        if ((unit != 0) && (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE)) {
            if (unit->unit_flags & UNIT_FLAG_EGG) {
                total++;
            } else if (!(*&unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                           & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
                && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
                    & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
                total++;
                unit_list_build_record(unit, &g_unit_list_records[shown], shown);
                g_unit_list_shown_unit_indices[shown] = shown;
                shown++;
            }
        }
    }

    for (i = 0; i < shown; i++) {
        g_unit_list_records[i].unit_count = total;
    }
    g_unit_list_count = shown;
    unit_list_sort(sort_mode);
    return shown;
}

/* Bubble-sort g_unit_list_shown_unit_indices (the shown-unit index list) by the
 * successive keys of g_menu_sort_orders.unit_list (the unit sort order the
 * reorder list edits, each key a record field picked through g_unit_list_sort_field_indices; a tie
 * moves on to the next key), then publish the sorted record pointers into
 * g_unit_list_pointers. Keys 1 and 4 and up sort in descending order. `sort_mode` is
 * not read. */
void unit_list_sort(s32 sort_mode) {
    s32 count;
    s32 j;
    s32 k;
    s32 diff;
    s32 lhs;
    s32 rhs;
    s16* rec; /* Native sort keys select signed 16-bit values within the record. */

    for (count = g_unit_list_count - 1; count > 0; count--) {
        for (j = 0; j < count; j++) {
            k = 0;
            do {
                rec = (s16*)&g_unit_list_records[g_unit_list_shown_unit_indices[j]];
                lhs = rec[g_unit_list_sort_field_indices[g_menu_sort_orders.unit_list[k]]];
                rec = (s16*)&g_unit_list_records[g_unit_list_shown_unit_indices[j + 1]];
                rhs = rec[g_unit_list_sort_field_indices[g_menu_sort_orders.unit_list[k]]];
                diff = rhs - lhs;
                k++;
            } while (diff == 0);
            if (g_menu_sort_orders.unit_list[k - 1] == 1 || g_menu_sort_orders.unit_list[k - 1] >= 4) {
                diff = -diff;
            }
            if (diff < 0) {
                s32 swap = g_unit_list_shown_unit_indices[j];
                g_unit_list_shown_unit_indices[j] = g_unit_list_shown_unit_indices[j + 1];
                g_unit_list_shown_unit_indices[j + 1] = swap;
            }
        }
    }
    for (j = 0; j < g_unit_list_count; j++) {
        g_unit_list_pointers[j] = &g_unit_list_records[g_unit_list_shown_unit_indices[j]];
    }
}

/* Copies the first signed-byte order list into a -1-terminated s16 list. */
void unit_list_load_sort_order(s32 unused, s16* entries) {
    s32 i;

    for (i = 0; (*entries = g_menu_sort_orders.unit_list[i]) != -1; i++) {
        entries++;
    }
}

/* Copy the low byte of each 16-bit entry into g_menu_sort_orders.unit_list
 * (the first, 12-byte list of those tables) until the terminator (-1) has been copied.
 * The first parameter is unused. */
void unit_list_store_sort_order(s32 unused, const s16* entries) {
    s32 index = 0;
    s8 value = *entries;

    g_menu_sort_orders.unit_list[index] = value;
    while (value != -1) {
        entries++;
        value = *entries;
        index += 1;
        g_menu_sort_orders.unit_list[index] = value;
    }
}

/* Find the Unit List record with a battle id, or return the active record count.
 *
 * The not-found result doubles as the next/end index for both known callers. */
s32 unit_list_find_by_battle_id(s16 battle_id) {
    s32 stack_scratch[2]; /* Unused; the target keeps an 8-byte leaf frame. */
    s32 count;
    s32 i;
    s32 search_value;
    unit_list_record_t** units;

    count = g_unit_list_count;
    i = 0;
    if (count > 0) {
        search_value = battle_id;
        units = g_unit_list_pointers;
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
