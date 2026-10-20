/* Status job page, job-value calculators and cached ability-page callbacks. */

#include "fft/job_list.h"
#include "fft/ability_list.h"
#include "fft/help.h"
#include "fft/job.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/sound.h"
#include "fft/status_screen.h"
#include "fft/text.h"
#include "psx/pad.h"
#include "psx/types.h"

extern s32 g_job_list_menu_initialized;

/* Byte-aligned display streams, including the terminating opcode. */
extern u8 g_job_list_render_commands[216];

/* Job level of the unit in the job list's `index`th job, also left in
 * g_job_list_job_current_level; levels are packed two per byte (odd jobs in
 * the low nibble). */
s32 job_list_level_calculate(s32 index) {
    s32 generic_job = job_generic_index_get(g_job_list_job_ids[index]);

    g_job_list_job_current_level = g_status_screen_unit_data[0]->career.job_levels[generic_job >> 1];
    if (generic_job & 1) {
        g_job_list_job_current_level &= 0xf;
    } else {
        g_job_list_job_current_level >>= 4;
    }
    return g_job_list_job_current_level;
}

/* Also returns the value: callers read it through g_menu_row_callbacks. */
static s32 job_list_calculate_jp(s32 index) {
    s32 generic_job = job_generic_index_get(g_job_list_job_ids[index]);

    return g_job_current_jp = g_status_screen_unit_data[0]->career.job_points.current.values[generic_job];
}

/* Also returns the value: callers read it through g_menu_row_callbacks. */
s32 job_list_total_jp_calculate(s32 index) {
    s32 generic_job = job_generic_index_get(g_job_list_job_ids[index]);

    return g_job_current_total_jp = g_status_screen_unit_data[0]->career.job_points.total.values[generic_job];
}

/* JP needed for the next level of the job whose level
 * the current-level callback last cached: the table entry for levels
 * below 8, otherwise the previous value with the dashes flag set. The
 * `index`th job is looked up but its result is not used. */
s32 job_list_next_level_jp_calculate(s32 index) {
    s32 level;

    job_generic_index_get(g_job_list_job_ids[index]);
    level = g_job_list_job_current_level & 0xf;
    if (level >= UNIT_JOB_LEVEL_MAX) {
        g_job_list_job_next_level_jp_requirement |= MENU_NUMBER_VALUE_DASHES;
    } else {
        g_job_list_job_next_level_jp_requirement = g_job_jp_requirements_by_level[level + 1];
    }
    return g_job_list_job_next_level_jp_requirement;
}

/* JOBSTTS builds the unlearned list; its final argument is Boolean,
 * so the original 2 requests unlearned abilities only. */
s32 job_list_mastered_calculate(s32 index) {
    s16 ability_list[24];

    g_job_current_mastered
        = ability_list_build(0, g_job_list_job_ids[index], MENU_ABILITY_CATEGORY_ALL, ability_list, 2) == 0;
    return g_job_current_mastered;
}

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

    if (g_job_list_menu_initialized == 0) {
        g_menu_row_callbacks[0].value = job_list_level_calculate;
        g_menu_row_callbacks[2].value = job_list_calculate_jp;
        g_menu_row_callbacks[5].value = job_list_next_level_jp_calculate;
        g_menu_row_callbacks[6].value = job_list_total_jp_calculate;
        g_menu_row_callbacks[13].value = job_list_mastered_calculate;
        job_list_build(0, g_job_list_job_ids, 0);
        menu_selection_record_restore(4, &selected, &scroll, g_job_list_job_ids);
        menu_init_scrollable_list(
            g_job_list_job_ids, selected, scroll, g_text_section_pointers[TEXT_SECTION_JOB_NAMES]);
        g_job_list_menu_initialized = 1;
    }

    g_text_selection_id = g_job_list_job_ids[g_menu_list_selected_index] + HELP_TEXT_ID_JOB_BASE;
    mask = g_menu_input_primary_repeat;
    if (mask & PSX_PAD_CIRCLE) {
        g_job_list_selected_id = g_job_list_job_ids[g_menu_list_selected_index];
        job_list_level_calculate(g_menu_list_selected_index);
        job_list_calculate_jp(g_menu_list_selected_index);
        job_list_total_jp_calculate(g_menu_list_selected_index);
        /* A mastered job has no next level to show. */
        if (job_list_mastered_calculate(g_menu_list_selected_index) == 0) {
            job_list_next_level_jp_calculate(g_menu_list_selected_index);
        }
        menu_selection_record_set(4, g_menu_list_selected_index, g_menu_scroll_base_index, g_job_list_job_ids);
        g_job_list_menu_initialized = 0;
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        return MENU_JOB_LIST_STEP_ABILITIES;
    }
    menu_run_script_with_palette_mode(g_job_list_render_commands, mask, g_menu_input_lock_mode);
    return MENU_JOB_LIST_STEP_JOBS;
}

/* Cached job-value callbacks for the ability page; each ignores its row argument. */
s32 job_list_get_jp(s32 unused_row) {
    return g_job_current_jp;
}

s32 job_list_get_level(s32 unused_row) {
    return g_job_list_job_current_level;
}

s32 job_list_get_next_level_jp(s32 unused_row) {
    return g_job_list_job_next_level_jp_requirement;
}

s32 job_list_get_total_jp(s32 unused_row) {
    return g_job_current_total_jp;
}

s32 job_list_is_mastered(s32 unused_row) {
    return g_job_current_mastered;
}
