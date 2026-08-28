#include "fft/job.h"

void main_job_store_unlock_bitset(u8 destination[UNIT_UNLOCKED_JOB_BYTE_COUNT], u32 unlocked_jobs) {
    *destination++ = unlocked_jobs >> 16;
    *destination++ = unlocked_jobs >> 8;
    *destination = unlocked_jobs;
}
