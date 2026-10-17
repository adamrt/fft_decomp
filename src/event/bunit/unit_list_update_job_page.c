/* The per-frame handler of the unit list's job page and the chosen job's ability list. */

#include "fft/ability_list.h"
#include "fft/job_list.h"
#include "fft/menu_input.h"
#include "fft/menu_selection.h"
#include "fft/sound.h"
#include "fft/unit_list.h"

/* Per-frame handler of the unit list's job page: runs the job list and,
 * once a job is chosen, its ability list, and returns to the unit list when
 * either step reports MENU_JOB_LIST_STEP_CLOSE. */
void unit_list_update_job_page(void) {
    if (g_job_list_menu_active == 0) {
        g_job_list_menu_active = 1;
        g_job_list_menu_phase = MENU_JOB_LIST_STEP_JOBS;
        menu_set_selection_value(0, 0);
        menu_clear_selection_records();
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
    }
    if (g_job_list_menu_phase == MENU_JOB_LIST_STEP_JOBS) {
        g_job_list_menu_phase = job_list_run();
    } else if (g_job_list_menu_phase == MENU_JOB_LIST_STEP_ABILITIES) {
        g_job_list_menu_phase = ability_list_update();
    }
    if (g_job_list_menu_phase == MENU_JOB_LIST_STEP_CLOSE) {
        g_unit_list_current_menu = UNIT_LIST_MENU_UNIT_LIST;
        menu_input_clear_state();
        g_job_list_menu_active = 0;
    }
}
