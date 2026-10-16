/* Whether a listed job's skillset has no ability left to learn for the selected unit, cached for row callbacks. */

#include "fft/job.h"
#include "fft/menu.h"
#include "fft/menu_panel.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Caches and returns whether the `index`th job's skillset has nothing left
 * for the selected unit to learn (no unlearned ability counted). */
s32 job_calculate_current_mastered(s32 index) {
    g_job_current_mastered = ability_build_list(g_unit_selected_index, g_menu_job_ids[index], MENU_ABILITY_CATEGORY_ALL,
                                 0, ABILITY_LIST_MODE_COUNT_UNLEARNED)
        == 0;
    return g_job_current_mastered;
}
