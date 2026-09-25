#include "fft/event_bunit.h"
#include "psx/types.h"

void bunit_menu_update_job(void) {
    if (g_bunit_job_menu_active == 0) {
        g_bunit_job_menu_active = 1;
        g_bunit_job_menu_phase = 0;
        bunit_menu_set_selection_value(0, 0);
        bunit_menu_clear_selection_records();
        g_bunit_sound_queued_effect_id = MAIN_SFX_CONFIRM;
    }
    if (g_bunit_job_menu_phase == 0) {
        g_bunit_job_menu_phase = bunit_menu_run_job_list();
    } else if (g_bunit_job_menu_phase == 1) {
        g_bunit_job_menu_phase = bunit_run_ability_list_menu();
    }
    if (g_bunit_job_menu_phase == -1) {
        g_bunit_menu_current_menu = -1;
        bunit_input_clear_state();
        g_bunit_job_menu_active = 0;
    }
}
