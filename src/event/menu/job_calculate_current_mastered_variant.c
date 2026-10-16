#include "fft/event_bunit.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Caches and returns whether the `index`th job's skillset has nothing left
 * to list for the selected unit (an empty ability list in mode 3). */
s32 job_calculate_current_mastered_variant(s32 index) {
    g_job_current_mastered
        = ability_build_list_variant(g_unit_selected_index, g_menu_job_ids[index], MENU_ABILITY_CATEGORY_ALL, 0, 3)
        == 0;
    return g_job_current_mastered;
}
