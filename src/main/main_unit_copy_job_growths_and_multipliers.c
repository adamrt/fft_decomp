#include "fft/battle.h"
#include "fft/main.h"

void main_unit_copy_job_growths_and_multipliers(battle_stats_t* unit) {
    job_data_t* job = &g_job_data_pointer[unit->job_id];

    main_util_copy_byte_data(job->growths_multipliers, &unit->raw_stats[UNIT_RAW_STAT_DATA_BYTE_COUNT], 10);
}
