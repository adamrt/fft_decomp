/* The job list menu, recording the chosen job. */

#include "fft/event_jobstts.h"
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
#include "psx/pad.h"
#include "psx/types.h"

/* JOBSTTS job page for the viewed unit. The first call builds the unit's job list,
 * installs the job level, JP, next-level JP, total JP and mastered row callbacks the
 * render script reads, and restores the saved cursor (selection record 4, after the
 * four ability categories). The help text is the highlighted job's description.
 * CIRCLE caches the chosen job's figures, saves the cursor and returns
 * MENU_JOB_LIST_STEP_ABILITIES; otherwise the list script runs and the result is
 * MENU_JOB_LIST_STEP_JOBS (the entrypoint handles CROSS). */
s32 jobstts_menu_run_job_list(void) {
    s16 unused[8]; /* Preserves the original frame size. */
    s16 selected;
    s16 scroll;
    s32 mask;

    if (g_jobstts_job_list_menu_initialized == 0) {
        g_menu_row_callbacks[0].value = jobstts_job_calculate_current_level;
        g_menu_row_callbacks[2].value = jobstts_job_calculate_current_jp;
        g_menu_row_callbacks[5].value = menu_job_calculate_next_level_jp_requirement;
        g_menu_row_callbacks[6].value = jobstts_job_calculate_current_total_jp;
        g_menu_row_callbacks[13].value = jobstts_job_calculate_current_mastered;
        jobstts_job_build_unit_job_list(0, g_menu_job_ids, 0);
        menu_get_selection_record(4, &selected, &scroll, g_menu_job_ids);
        menu_init_scrollable_list(g_menu_job_ids, selected, scroll, g_text_section_pointers[TEXT_SECTION_JOB_NAMES]);
        g_jobstts_job_list_menu_initialized = 1;
    }

    /* The help text is the job's entry in HELP_TEXT_SECTION_JOBS (0x6000 = section 12). */
    g_text_selection_id = g_menu_job_ids[g_menu_list_selected_index] + 0x6000;
    mask = g_menu_input_primary_repeat;
    if (mask & PSX_PAD_CIRCLE) {
        g_job_selected_id = g_menu_job_ids[g_menu_list_selected_index];
        jobstts_job_calculate_current_level(g_menu_list_selected_index);
        jobstts_job_calculate_current_jp(g_menu_list_selected_index);
        jobstts_job_calculate_current_total_jp(g_menu_list_selected_index);
        /* A mastered job has no next level to show. */
        if (jobstts_job_calculate_current_mastered(g_menu_list_selected_index) == 0) {
            menu_job_calculate_next_level_jp_requirement(g_menu_list_selected_index);
        }
        menu_set_selection_record(4, g_menu_list_selected_index, g_menu_scroll_base_index, g_menu_job_ids);
        g_jobstts_job_list_menu_initialized = 0;
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        return MENU_JOB_LIST_STEP_ABILITIES;
    }
    jobstts_menu_run_script_with_palette_mode(&g_jobstts_job_list_render_commands, mask, g_menu_input_lock_mode);
    return MENU_JOB_LIST_STEP_JOBS;
}
