/* Resident stat rebuilding and level growth. Equipment and job changes preserve
 * inflicted statuses, reconcile their timers and publish the battle display changes. */

#include "fft/ability.h"
#include "fft/job.h"
#include "fft/memory.h"
#include "fft/party.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "psx/libc.h"
#include "psx/types.h"

static void status_timers_init(unit_stats_t* unit);

static void statuses_reconcile(unit_stats_t* unit);

static void stats_refresh(unit_stats_t* unit, s32 skip_status_check, s32 initializing);

/* Seed inflicted_status with the current statuses the unit does not have
 * innately, then set up the status CTs (status_timers_init) and reconcile the
 * status flags. */
void status_unit_init(unit_stats_t* unit) {
    s32 status_set;

    for (status_set = 0; status_set < STATUS_BYTE_COUNT; status_set++) {
        unit->inflicted_status[status_set]
            = unit->status_sets.current[status_set] & ~unit->status_sets.innate[status_set];
    }
    status_timers_init(unit);
    statuses_reconcile(unit);
}

/* Clear the timed-status CTs and start the death counter at 3, or at 0xff for
 * an immortal unit and for one with a save- or load-formation flag. */
static void status_timers_init(unit_stats_t* unit) {
    mem_zero(unit->status_ct, sizeof(unit->status_ct));
    if ((unit->team_flags & UNIT_TEAM_FLAG_IMMORTAL)
        || (unit->unit_flags & (UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION))) {
        unit->death_counter = 0xff;
    } else {
        unit->death_counter = 3;
    }
}

/* Apply Float and reconcile the unit's status flags and status CT. */
static void statuses_reconcile(unit_stats_t* unit) {
    s32 status;
    u8 immunity;

    if (unit->ability_flags.movement[2] & UNIT_MOVEMENT_SET_3_FLOAT) {
        unit->status_sets.innate[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] |= STATUS_BYTE_MASK(STATUS_ID_FLOAT);
    }

    for (status = 0; status < STATUS_BYTE_COUNT; status++) {
        immunity = unit->status_sets.immunity[status];
        immunity &= ~unit->status_sets.innate[status];
        unit->status_sets.immunity[status] = immunity;
        unit->status_sets.current[status] = unit->status_sets.innate[status] | unit->inflicted_status[status];
    }

    for (status = STATUS_ID_POISON; status < STATUS_COUNT; status++) {
        s32 status_byte = status / 8;
        s32 status_bit = status & 7;

        if ((unit->inflicted_status[status_byte] & (0x80 >> status_bit))
            && unit->status_ct[STATUS_CT_INDEX(status)] == 0) {
            status_ct_set(unit, status, 0);
        }
    }
}

/* stats_refresh with skip_status_check set: stats
 * are re-derived without the critical and status notifications. */
void unit_formation_stats_refresh(unit_stats_t* unit) {
    stats_refresh(unit, 1, 0);
}

/* stats_refresh with both flags clear: stats are
 * re-derived and the status notifications run. */
void unit_stats_refresh(unit_stats_t* unit) {
    stats_refresh(unit, 0, 0);
}

/* stats_refresh in its initializing form: the
 * inflicted statuses are seeded from the current ones and their CTs cleared.
 * Nothing in the tree calls it. */
void unit_stats_init(unit_stats_t* unit) {
    stats_refresh(unit, 0, 1);
}

/* Re-derives the job-dependent stats (move/jump, equippable categories,
 * innate/immune/starting statuses, elemental affinity, R/S/M flags and
 * equipment bonuses), then reconciles the status bits.  `skip_status_check`
 * leaves the critical/status notifications alone; `initializing` seeds the
 * inflicted statuses from the current ones and clears their CTs. */
static void stats_refresh(unit_stats_t* unit, s32 skip_status_check, s32 initializing) {
    u8 saved_inflicted[STATUS_BYTE_COUNT];
    u8 saved_current[STATUS_BYTE_COUNT];
    s32 i;
    s32 status_id;
    s32 mask;
    s32 byte_index;
    s32 current_bit;
    s32 battle_id = unit->battle_id;
    s32 saved_bit;
    u8 formation_index;
    job_data_t* job;

    status_current_update(unit);
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        saved_current[i] = unit->status_sets.current[i];
        saved_inflicted[i] = unit->inflicted_status[i];
    }
    job = &g_job_data_pointer[unit->job_id];
    unit->move = job->move;
    unit->jump = job->jump & JOB_JUMP_VALUE_MASK;
    mem_copy_bytes_source_first(
        job->equipment_categories, unit->equipment_categories, sizeof(job->equipment_categories));
    mem_copy_bytes_source_first(&job->status_sets, &unit->status_sets, sizeof(job->status_sets));
    mem_copy_bytes_source_first(job->elemental_affinity, unit->elemental_affinity, sizeof(job->elemental_affinity));
    unit->elemental_affinity[ELEMENTAL_AFFINITY_STRENGTHEN] = 0;
    unit_rsm_flags_enable(unit);
    unit_move_jump_calculate(unit, 1);
    unit_equipment_stats_set(unit);
    unit_equipment_attributes_set(unit, 0);
    unit_equippable_items_set(unit);

    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        if (initializing) {
            unit->inflicted_status[i] = unit->status_sets.current[i];
        } else {
            unit->inflicted_status[i] = saved_inflicted[i];
        }
    }

    formation_index = unit->formation_index;
    unit->formation_index = UNIT_FORMATION_INDEX_NONE;
    statuses_reconcile(unit);
    unit->formation_index = formation_index;
    status_current_update(unit);

    if (skip_status_check != 0) {
        return;
    }
    if ((unit->max_hp / 5) >= unit->hp) {
        unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)] |= STATUS_BYTE_MASK(STATUS_ID_CRITICAL);
    } else {
        unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)] &= ~STATUS_BYTE_MASK(STATUS_ID_CRITICAL);
    }
    status_current_update(unit);

    for (i = 0, status_id = STATUS_HANDLER_INDEX(STATUS_ID_UNNAMED_00); i < STATUS_COUNT; i++, status_id++) {
        byte_index = i / 8;
        mask = 0x80 >> (i & 7);
        saved_bit = saved_current[byte_index] & mask;
        current_bit = unit->status_sets.current[byte_index] & mask;
        if (initializing != 0 || saved_bit != current_bit) {
            if (current_bit != 0) {
                if (initializing != 0) {
                    status_ct_set(unit, i, 0);
                }
                status_queue_display_flag_change_if_executing(status_id, STATUS_DISPLAY_CHANGE_ADD, battle_id);
            } else {
                status_queue_display_flag_change_if_executing(status_id, STATUS_DISPLAY_CHANGE_REMOVE, battle_id);
            }
        }
    }
}

/* Level up at 100 EXP: one level of growth, and EXP goes back to 0 (the excess
 * is lost). At level 99 EXP is held at 99 instead. Returns 1 when the level
 * rose. */
s32 unit_level_up_check(unit_stats_t* unit) {
    u8 level;

    if (unit->experience < UNIT_LEVEL_UP_EXPERIENCE) {
        return 0;
    }

    level = unit->level;
    if (level >= UNIT_LEVEL_MAX) {
        unit->experience = UNIT_LEVEL_EXPERIENCE_MAX;
        return 0;
    }

    unit_level_growth_apply(unit, 0);
    unit->experience = 0;
    unit->level = level + 1;
    return 1;
}

/* Raises a roster unit by `level_delta` levels (capped at 99), growing each
 * 24-bit raw stat by raw / (level + growth) per level, and returns the new
 * level.  The growth table is the job's stat_growths pairs. */
u32 party_levels_gain(party_unit_t* party_unit, s32 level_delta) {
    u32 current_level = party_unit->level;
    u32 level = current_level + level_delta;
    job_stat_growth_t* growth_table;
    unit_raw_stat_t* raw_stats;
    s32 levels_to_gain;
    s32 level_index;
    s32 i;
    u32 raw;
    u32 divisor;

    raw_stats = party_unit->raw_stats;
    growth_table = g_job_data_pointer[party_unit->job_id].stat_growths;
    if (level > UNIT_LEVEL_MAX) {
        level = UNIT_LEVEL_MAX;
    }
    levels_to_gain = level - current_level;
    if (levels_to_gain <= 0) {
        return level;
    }
    for (level_index = 0; level_index < levels_to_gain; level_index++) {
        for (i = 0; i < UNIT_RAW_STAT_COUNT; i++) {
            level = party_unit->level;
            raw = raw_stats[i].low + (raw_stats[i].middle << 8) + (raw_stats[i].high << 16);
            if (growth_table[i].growth == 0) {
                divisor = level + 1;
            } else {
                divisor = level + growth_table[i].growth;
            }
            raw += raw / divisor;
            if (raw > 0xffffff) {
                raw = 0xffffff;
            }
            raw_stats[i].low = raw;
            raw_stats[i].middle = raw >> 8;
            raw_stats[i].high = raw >> 16;
        }
        party_unit->level++;
    }
    return party_unit->level;
}

/* Applies one level of growth (or its inverse for a level down) to each
 * 24-bit raw stat: raw +/- raw / (growth + level), a zero growth counting as
 * 1, capped at 0xffffff; then refreshes the derived stats and clamps HP/MP. */
void unit_level_growth_apply(unit_stats_t* unit, s32 level_down) {
    unit_raw_stat_t* raw_stats = unit->raw_stats;
    job_stat_growth_t* growth = unit->stat_growths;
    u32 value;
    u32 divisor;
    s32 level = unit->level;
    u32 minimum_growth;
    s32 i;

    for (i = 0; i < UNIT_RAW_STAT_COUNT; i++) {
        divisor = growth[i].growth;
        value = raw_stats[i].low + (raw_stats[i].middle << 8) + (raw_stats[i].high << 16);
        minimum_growth = divisor;
        if (divisor == 0) {
            minimum_growth = 1;
        }
        divisor = minimum_growth + level;
        /* Drawn and discarded: each stat's growth consumes one random number. */
        rand();
        if (level_down) {
            value -= value / divisor;
        } else {
            value += value / divisor;
        }
        if (value > 0x00ffffff) {
            value = 0x00ffffff;
        }
        raw_stats[i].low = value;
        raw_stats[i].middle = value >> 8;
        raw_stats[i].high = value >> 16;
    }

    unit_stats_refresh(unit);
    if (unit->hp > unit->max_hp) {
        unit->hp = unit->max_hp;
    }
    if (unit->mp > unit->max_mp) {
        unit->mp = unit->max_mp;
    }
}

/* Start a timed status's CT from g_status_effect_data, or clear it when
 * removing. For Dead, in either case, the death counter is set to 3, or to
 * 0xff for an immortal unit and for one with a save- or load-formation flag.
 * Returns -1 when Death Sentence is already counting, so a second one does not
 * restart it; otherwise 0. */
s32 status_ct_set(unit_stats_t* unit, s32 status_id, s32 removing) {
    u32 ct_index;
    s32 stored_index;

    if (status_id == STATUS_ID_DEAD) {
        if ((unit->team_flags & UNIT_TEAM_FLAG_IMMORTAL)
            || (unit->unit_flags & (UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION))) {
            unit->death_counter = 0xff;
        } else {
            unit->death_counter = 3;
        }
    }

    ct_index = STATUS_CT_INDEX(status_id);
    if (ct_index >= STATUS_TIMED_COUNT) {
        return 0;
    }
    stored_index = ct_index;
    if (removing) {
        unit->status_ct[stored_index] = 0;
        return 0;
    }
    if ((stored_index == STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE))
        && (unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)] != 0)) {
        return -1;
    }
    unit->status_ct[stored_index] = g_status_effect_data[status_id].ct;
    return 0;
}
