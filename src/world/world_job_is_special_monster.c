#include "fft/world.h"
#include "psx/types.h"

s32 world_job_is_special_monster(s32 job_id) {
    s32 result;

    result = (u32)(job_id - JOB_ID_SPECIAL_MONSTER_1_FIRST) < JOB_ID_SPECIAL_MONSTER_1_COUNT;
    if ((u32)(job_id - JOB_ID_SPECIAL_MONSTER_2_FIRST) < JOB_ID_SPECIAL_MONSTER_2_COUNT) {
        result = 1;
    }
    return result;
}
