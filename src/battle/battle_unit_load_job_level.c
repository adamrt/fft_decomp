#include "fft/battle.h"
#include "fft/job.h"
#include "psx/types.h"

/*
 * Returns the unit's level (0..8) in the generic job job_id, from the
 * nibble-packed battle_stats_t.job_levels bytes at 0xd2.
 * *out_slot receives the zero-based job slot (Chemist = 1 ... Mime = 19),
 * or 0 for Squire and non-generic jobs.
 */
s32 battle_unit_load_job_level(battle_stats_t* unit, s32 job_id, s32* out_slot) {
    s32 slot;
    s32 packed;
    if ((u32)(job_id - JOB_ID_CHEMIST) < (JOB_ID_GENERIC_LAST - JOB_ID_CHEMIST + 1)) {
        *out_slot = job_id - JOB_ID_SQUIRE;
    } else {
        *out_slot = 0;
    }
    slot = *out_slot;
    packed = unit->job_levels[slot / 2];
    /* Reassigning packed in both arms keeps the target's `sra`; returning
     * `packed >> 4` directly lets GCC use the byte's range and emit `srl`. */
    if (!(slot & 1)) {
        packed >>= 4;
    } else {
        packed &= 0xF;
    }
    return packed;
}
