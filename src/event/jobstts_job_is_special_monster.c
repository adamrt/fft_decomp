#include "fft/job.h"
#include "psx/types.h"

s32 jobstts_job_is_special_monster(s32 job_id) {
    s32 monster_job = job_id - JOB_ID_SPECIAL_MONSTER_1_FIRST;
    s32 special_job = job_id - JOB_ID_SPECIAL_MONSTER_2_FIRST;

    if ((u32)special_job < JOB_ID_SPECIAL_MONSTER_2_COUNT) {
        return 1;
    }
    return (u32)monster_job < JOB_ID_SPECIAL_MONSTER_1_COUNT;
}
