/* Whether a listed job's skillset has no ability left to learn, cached for row callbacks. */

#include "fft/ability_list.h"
#include "fft/job.h"
#include "fft/job_list.h"
#include "psx/types.h"

/* Caches and returns whether the `index`th job's skillset has nothing left
 * to list (an empty ability list in mode 2). */
s32 job_list_calculate_mastered(s32 index) {
    s16 ability_list[24];

    g_job_current_mastered
        = ability_list_build(0, g_job_list_job_ids[index], MENU_ABILITY_CATEGORY_ALL, ability_list, 2) == 0;
    return g_job_current_mastered;
}
