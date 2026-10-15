#include "fft/main.h"

/* Compute the packed job levels from the JP totals: one byte per pair of jobs,
 * the first job's level in the high nibble. */
void main_unit_init_job_levels(const u16* unit_job_jp, u8* unit_job_levels) {
    s32 i;
    s32 level;

    for (i = 0; i < UNIT_JOB_LEVEL_BYTE_COUNT; i++) {
        level = main_job_calculate_level(unit_job_jp[i * UNIT_JOBS_PER_LEVEL_BYTE]) << 4;
        unit_job_levels[i] = level | main_job_calculate_level(unit_job_jp[i * UNIT_JOBS_PER_LEVEL_BYTE + 1]);
    }
}
