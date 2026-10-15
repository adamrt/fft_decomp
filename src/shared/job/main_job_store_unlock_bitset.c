#include "fft/main.h"

/* Store the low 24 bits of unlocked_jobs as three bytes, most significant first. */
void main_job_store_unlock_bitset(u8 destination[UNIT_UNLOCKED_JOB_BYTE_COUNT], u32 unlocked_jobs) {
    *destination++ = unlocked_jobs >> 16;
    *destination++ = unlocked_jobs >> 8;
    *destination = unlocked_jobs;
}
