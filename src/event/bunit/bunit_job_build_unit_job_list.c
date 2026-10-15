#include "fft/event_bunit.h"
#include "fft/main.h"
#include "psx/types.h"

/*
 * Writes a unit's selectable jobs to a -1-terminated list and returns their
 * count. BUNIT copy of world_job_build_unit_list without its list modes.
 *
 * The base job comes first, followed by the generic jobs whose unlocked bit
 * is set (Bard excluded for female units, Dancer for male units); special
 * monsters and units using monster skillsets get only the base job. The
 * third argument is not read.
 */
s32 bunit_job_build_unit_job_list(s32 unit_id, s16* job_ids, s32 unused_2) {
    s32 saved_unit_id = unit_id;
    u32 unit_index = (u8)saved_unit_id;
    bunit_unit_t** unit_table = g_bunit_unit_pointers;
    bunit_unit_t** slot;
    s16* out;
    s16* job_ids_output;
    bunit_unit_t* unit;
    bunit_unit_t** unit_data_cursor;
    s32 job_count;
    s32 generic_job_index;

    slot = &unit_table[unit_index];
    unit = *slot;
    job_ids_output = job_ids;
    bunit_bits_init_reader(unit->unlocked_jobs);
    job_ids_output[0] = bunit_job_get_base(saved_unit_id & 0xff);
    job_count = 1;
    if (job_is_special_monster((*slot)->job_id) || (*slot)->uses_monster_skillset != 0) {
        job_ids_output[1] = -1;
        return 1;
    }

    bunit_bits_read(1);
    generic_job_index = 1;
    unit_data_cursor = slot;
    out = job_ids_output + 1;
    for (; generic_job_index < JOB_ID_GENERIC_COUNT; generic_job_index++) {
        s32 job_is_unlocked;

        job_is_unlocked = bunit_bits_read(1);
        saved_unit_id = generic_job_index + JOB_ID_SQUIRE;
        if (saved_unit_id == JOB_ID_BARD && ((*unit_data_cursor)->gender_flags & UNIT_FLAG_FEMALE) != 0) {
            continue;
        }
        if (saved_unit_id == JOB_ID_DANCER && ((*unit_data_cursor)->gender_flags & UNIT_FLAG_MALE) != 0) {
            continue;
        }
        if (job_is_unlocked != 0) {
            *out = saved_unit_id;
            out++;
            job_count++;
        }
    }
    job_ids_output[job_count] = -1;
    return job_count;
}
