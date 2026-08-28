#include "fft/job.h"
#include "fft/jobstts.h"

s32 jobstts_job_build_unit_job_list(u8 unit_id, s16* job_ids, s32 unused) {
    u32 unit_index = (u8)unit_id;
    jobstts_unit_job_data_t** unit_table = g_jobstts_unit_data;
    s16* out;
    s32 count;
    s32 i;
    s32 job;
    jobstts_unit_job_data_t** slot;
    jobstts_unit_job_data_t** loop_slot;

    (void)unused;

    slot = &unit_table[unit_index];
    jobstts_bits_init_primary_reader((*slot)->unlocked_jobs);
    job_ids[0] = jobstts_job_get_base(unit_id);
    if ((jobstts_job_is_special_monster((*slot)->job_id) != 0)
        || ((*slot)->character_identity == CHARACTER_IDENTITY_MONSTER)) {
        job_ids[1] = -1;
        return 1;
    }
    jobstts_bits_read_primary(1);
    count = 1;
    i = 1;
    loop_slot = slot;
    out = job_ids + 1;
    for (; i < JOB_ID_GENERIC_COUNT; i++) {
        job = i + JOB_ID_SQUIRE;
        if ((jobstts_bits_read_primary(1) != 0)
            && ((job != JOB_ID_BARD) || !((*loop_slot)->unit_flags & UNIT_FLAG_FEMALE))
            && ((job != JOB_ID_DANCER) || !((*loop_slot)->unit_flags & UNIT_FLAG_MALE))) {
            *out = job;
            out++;
            count++;
        }
    }
    job_ids[count] = -1;
    return count;
}
