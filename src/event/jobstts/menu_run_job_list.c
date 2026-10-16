/* The job list menu, recording the chosen job. */

#include "fft/event_jobstts.h"
#include "fft/job.h"
#include "fft/main_sound.h"
#include "fft/menu_help.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
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
s32 job_list_run(void) {
    s16 unused[8]; /* Preserves the original frame size. */
    s16 selected;
    s16 scroll;
    s32 mask;

    if (g_job_list_list_menu_initialized == 0) {
        g_menu_row_callbacks[0].value = job_list_calculate_level;
        g_menu_row_callbacks[2].value = job_list_calculate_jp;
        g_menu_row_callbacks[5].value = job_list_calculate_next_level_jp;
        g_menu_row_callbacks[6].value = job_list_calculate_total_jp;
        g_menu_row_callbacks[13].value = job_list_calculate_mastered;
        job_list_build(0, g_job_list_job_ids, 0);
        menu_get_selection_record(4, &selected, &scroll, g_job_list_job_ids);
        menu_init_scrollable_list(
            g_job_list_job_ids, selected, scroll, g_text_section_pointers[TEXT_SECTION_JOB_NAMES]);
        g_job_list_list_menu_initialized = 1;
    }

    g_text_selection_id = g_job_list_job_ids[g_menu_list_selected_index] + HELP_TEXT_ID_JOB_BASE;
    mask = g_menu_input_primary_repeat;
    if (mask & PSX_PAD_CIRCLE) {
        g_job_list_selected_id = g_job_list_job_ids[g_menu_list_selected_index];
        job_list_calculate_level(g_menu_list_selected_index);
        job_list_calculate_jp(g_menu_list_selected_index);
        job_list_calculate_total_jp(g_menu_list_selected_index);
        /* A mastered job has no next level to show. */
        if (job_list_calculate_mastered(g_menu_list_selected_index) == 0) {
            job_list_calculate_next_level_jp(g_menu_list_selected_index);
        }
        menu_set_selection_record(4, g_menu_list_selected_index, g_menu_scroll_base_index, g_job_list_job_ids);
        g_job_list_list_menu_initialized = 0;
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        return MENU_JOB_LIST_STEP_ABILITIES;
    }
    menu_run_script_with_palette_mode(&g_job_list_list_render_commands, mask, g_menu_input_lock_mode);
    return MENU_JOB_LIST_STEP_JOBS;
}
