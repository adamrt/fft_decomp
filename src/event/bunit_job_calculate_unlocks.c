#include "fft/event_bunit.h"
#include "psx/types.h"

/* Reads the saved-party layout (gender +0x04, packed levels +0x64), not
 * the BUNIT menu record. Target: 0x801c7278. */
void bunit_job_calculate_unlocks(party_data_t* unit, u8* job_unlocks) {
    s32 unlocks
        = main_job_calculate_unlocked(unit->job_levels, unit->gender_flags & (UNIT_FLAG_FEMALE | UNIT_FLAG_MALE));

    job_unlocks[0] = unlocks >> 16;
    job_unlocks[1] = unlocks >> 8;
    job_unlocks[2] = unlocks;
}
