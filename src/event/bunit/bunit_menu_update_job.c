#include "fft/event_bunit.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/sound.h"

/* Per-frame handler of the unit list's job page: runs the job list and,
 * once a job is chosen, its ability list, and returns to the unit list when
 * either step reports MENU_JOB_LIST_STEP_CLOSE. */
void bunit_menu_update_job(void) {
    if (g_bunit_job_menu_active == 0) {
        g_bunit_job_menu_active = 1;
        g_bunit_job_menu_phase = MENU_JOB_LIST_STEP_JOBS;
        menu_set_selection_value(0, 0);
        menu_clear_selection_records();
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
    }
    if (g_bunit_job_menu_phase == MENU_JOB_LIST_STEP_JOBS) {
        g_bunit_job_menu_phase = menu_run_job_list_variant();
    } else if (g_bunit_job_menu_phase == MENU_JOB_LIST_STEP_ABILITIES) {
        g_bunit_job_menu_phase = menu_ability_update_list_menu_variant();
    }
    if (g_bunit_job_menu_phase == MENU_JOB_LIST_STEP_CLOSE) {
        g_bunit_menu_current_menu = BUNIT_MENU_UNIT_LIST;
        menu_input_clear_state();
        g_bunit_job_menu_active = 0;
    }
}
