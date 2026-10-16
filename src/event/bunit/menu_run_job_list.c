/* The unit list's job list for the selected unit, with job level and JP row callbacks. */

#include "fft/event_bunit.h"
#include "fft/job.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/menu_help.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/unit_party.h"
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
s32 job_list_run(void) {
    s16 selected;
    s16 scroll;
    s32 mask;

    if (g_job_list_list_initialized == 0) {
        menu_set_vsync_mode_and_event_speed(2);
        unit_list_build_job_list(g_unit_selected_index & 0xff, g_job_list_job_ids, 0);
        g_menu_row_callbacks[0].value = job_list_calculate_level;
        g_menu_row_callbacks[2].value = job_list_calculate_jp;
        g_menu_row_callbacks[5].value = job_list_calculate_next_level_jp;
        g_menu_row_callbacks[6].value = job_list_calculate_total_jp;
        g_menu_row_callbacks[13].value = job_list_calculate_mastered;
        menu_get_selection_record(9, &selected, &scroll, g_job_list_job_ids);
        menu_init_scrollable_list(
            g_job_list_job_ids, selected, scroll, g_text_section_pointers[TEXT_SECTION_JOB_NAMES]);
        g_job_list_list_initialized = 1;
    }

    g_text_selection_id = g_job_list_job_ids[g_menu_list_selected_index] + HELP_TEXT_ID_JOB_BASE;
    mask = g_menu_input_primary_repeat;
    if (mask & PSX_PAD_CROSS) {
        menu_set_vsync_mode_and_event_speed(0);
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        g_job_list_list_initialized = 0;
        return MENU_JOB_LIST_STEP_CLOSE;
    }
    if ((mask & PSX_PAD_CIRCLE) && g_unit_list_current_menu == UNIT_LIST_MENU_JOB) {
        g_job_list_selected_id = g_job_list_job_ids[g_menu_list_selected_index];
        g_job_list_selected_generic_index = job_get_generic_index(g_job_list_selected_id);
        job_list_calculate_level(g_menu_list_selected_index);
        job_list_calculate_jp(g_menu_list_selected_index);
        job_list_calculate_total_jp(g_menu_list_selected_index);
        job_list_calculate_next_level_jp(g_menu_list_selected_index);
        menu_set_selection_record(9, g_menu_list_selected_index, g_menu_scroll_base_index, g_job_list_job_ids);
        g_job_list_list_initialized = 0;
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        return MENU_JOB_LIST_STEP_ABILITIES;
    }
    menu_run_script_with_palette_mode(
        &g_job_list_list_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    return MENU_JOB_LIST_STEP_JOBS;
}
