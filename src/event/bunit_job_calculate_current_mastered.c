#include "fft/bunit.h"
#include "psx/types.h"

s32 bunit_job_calculate_current_mastered(s32 index) {
    g_bunit_job_current_mastered
        = bunit_create_ability_list(g_bunit_unit_selected_index, g_bunit_job_ids[index], 15, 0, 3) == 0;
    return g_bunit_job_current_mastered;
}
