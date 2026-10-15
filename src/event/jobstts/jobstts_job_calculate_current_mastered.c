#include "fft/event_jobstts.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Caches and returns whether the `index`th job's skillset has nothing left
 * to list (an empty ability list in mode 2). */
s32 jobstts_job_calculate_current_mastered(s32 index) {
    s16 ability_list[24];

    g_jobstts_job_current_mastered
        = jobstts_ability_build_list(0, g_menu_job_ids[index], MENU_ABILITY_CATEGORY_ALL, ability_list, 2) == 0;
    return g_jobstts_job_current_mastered;
}
