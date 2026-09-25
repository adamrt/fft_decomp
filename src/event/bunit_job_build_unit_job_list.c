#include "fft/event_bunit.h"
#include "psx/types.h"

s32 bunit_job_build_unit_job_list(s32 unit_id, s16* job_ids) {
    s32 saved_unit_id = unit_id;
    u32 unit_index = (u8)saved_unit_id;
    bunit_unit_data_t** unit_table = g_bunit_unit_data;
    void* working_pointer;
    s16* job_ids_output;
    bunit_unit_data_t* unit_data;
    bunit_unit_data_t** unit_data_cursor;
    s32 job_count;
    s32 generic_job_index;

    working_pointer = &unit_table[unit_index];
    unit_data = *(bunit_unit_data_t**)working_pointer;
    job_ids_output = job_ids;
    bunit_bits_init_reader(unit_data->unlocked_jobs);
    job_ids_output[0] = bunit_job_get_base(saved_unit_id & 0xff);
    job_count = 1;
    if (bunit_job_is_special_monster((*(bunit_unit_data_t**)working_pointer)->monster_base_job_id)
        || (*(bunit_unit_data_t**)working_pointer)->uses_monster_skillset != 0) {
        job_ids_output[1] = -1;
        return 1;
    }

    bunit_bits_read(1);
    generic_job_index = 1;
    unit_data_cursor = (bunit_unit_data_t**)working_pointer;
    working_pointer = job_ids_output + 1;
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
            *(s16*)working_pointer = saved_unit_id;
            working_pointer = (s16*)working_pointer + 1;
            job_count++;
        }
    }
    job_ids_output[job_count] = -1;
    return job_count;
}
