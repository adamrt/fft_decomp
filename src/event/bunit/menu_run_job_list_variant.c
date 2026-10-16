#include "fft/event_bunit.h"
#include "fft/job.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_panel.h"
#include "fft/menu_script.h"
#include "fft/menu_text.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/unit.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Runs the unit list's job list for the selected unit.
 *
 * The first call builds the unit's job list and installs the job level, JP,
 * next-level, total-JP and mastered row callbacks. Cross returns
 * MENU_JOB_LIST_STEP_CLOSE; Circle, while the job menu is the current menu,
 * caches the job's id, generic index and JP figures and returns
 * MENU_JOB_LIST_STEP_ABILITIES; otherwise the list script runs and the
 * result is MENU_JOB_LIST_STEP_JOBS. */
s32 menu_run_job_list_variant(void) {
    s16 out_a;
    s16 out_b;
    s32 mask;

    if (g_bunit_job_list_initialized == 0) {
        menu_set_vsync_mode_and_event_speed(2);
        bunit_job_build_unit_job_list(g_unit_selected_index & 0xff, g_menu_job_ids, 0);
        g_menu_row_callbacks[0].value = job_calculate_current_level_variant;
        g_menu_row_callbacks[2].value = job_calculate_current_jp_variant;
        g_menu_row_callbacks[5].value = menu_job_calculate_next_level_jp_requirement;
        g_menu_row_callbacks[6].value = job_calculate_current_total_jp_variant;
        g_menu_row_callbacks[13].value = job_calculate_current_mastered_variant;
        menu_get_selection_record_variant(9, &out_a, &out_b, g_menu_job_ids);
        menu_init_scrollable_list(g_menu_job_ids, out_a, out_b, g_text_section_pointers[TEXT_SECTION_JOB_NAMES]);
        g_bunit_job_list_initialized = 1;
    }

    g_text_selection_id = g_menu_job_ids[g_menu_list_selected_index] + 0x6000;
    mask = g_menu_input_primary_repeat;
    if (mask & PSX_PAD_CROSS) {
        menu_set_vsync_mode_and_event_speed(0);
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        g_bunit_job_list_initialized = 0;
        return MENU_JOB_LIST_STEP_CLOSE;
    }
    if ((mask & PSX_PAD_CIRCLE) && g_bunit_menu_current_menu == BUNIT_MENU_JOB) {
        g_job_selected_id = g_menu_job_ids[g_menu_list_selected_index];
        g_bunit_job_selected_generic_index = job_get_generic_index(g_job_selected_id);
        job_calculate_current_level_variant(g_menu_list_selected_index);
        job_calculate_current_jp_variant(g_menu_list_selected_index);
        job_calculate_current_total_jp_variant(g_menu_list_selected_index);
        menu_job_calculate_next_level_jp_requirement(g_menu_list_selected_index);
        menu_set_selection_record(9, g_menu_list_selected_index, g_menu_scroll_base_index, g_menu_job_ids);
        g_bunit_job_list_initialized = 0;
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        return MENU_JOB_LIST_STEP_ABILITIES;
    }
    menu_run_script_with_palette_mode_variant(
        &g_bunit_job_list_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    return MENU_JOB_LIST_STEP_JOBS;
}
