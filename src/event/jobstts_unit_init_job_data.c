#include "fft/event_jobstts.h"

void jobstts_unit_init_job_data(s32 unit_id) {
    jobstts_unit_copy_job_data(battle_unit_get_stats_from_battle_id(unit_id), &g_jobstts_unit_job_data);
    g_jobstts_unit_data[0] = &g_jobstts_unit_job_data;
}
