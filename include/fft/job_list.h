#ifndef JOB_LIST_H
#define JOB_LIST_H

/* The job page that the Unit List, the Status screen and Formation show: building the list and its row values. */

#include "fft/menu_script.h"
#include "psx/types.h"

/* job list */
/* Step of the job list menus (WORLD learn and view-abilities menus, BUNIT job menu, the JOBSTTS page); each list's
 * handler returns the next step. In JOBSTTS, JOBS is job_list_run, ABILITIES is ability_list_update and CLOSE makes
 * jobstts_entrypoint close the screen. */
typedef enum menu_job_list_step {
    MENU_JOB_LIST_STEP_CLOSE = -1,
    MENU_JOB_LIST_STEP_JOBS = 0,
    MENU_JOB_LIST_STEP_ABILITIES = 1
} menu_job_list_step_e;

extern u8 g_job_list_initialized;
extern s32 g_job_list_menu_initialized;
extern s8 g_job_list_menu_active;
extern s8 g_job_list_menu_phase; /* menu_job_list_step_e */
extern s16 g_job_list_selected_generic_index;

/* The job highlighted in the job list. */
extern s16 g_job_list_selected_id;

s32 job_list_run(void);
s32 job_list_get_base(s16 unit_id);

/* WORLD and JOBSTTS each build their own body. */
s32 job_list_build(u8 unit_index, s16* job_ids, s32 mode);

/* row callbacks */
/* Row callbacks of the job lists: the menu script calls them for a row's level and JP numbers. */
extern s16 g_job_list_job_ids[];
extern s32 g_job_list_job_current_level;
extern u32 g_job_list_job_next_level_jp_requirement;

/* The JP, total JP and mastery the job list calculators last cached for the row being drawn. */
extern u32 g_job_current_jp;
extern u32 g_job_current_total_jp;
extern s32 g_job_current_mastered;

s32 job_list_calculate_level(s32 index);
s32 job_list_calculate_total_jp(s32 index);

/* Caches and returns whether the index-th job's skillset has nothing left to learn for the selected unit. */
s32 job_list_calculate_mastered(s32 index);

/* JP needed for the next level of the job whose level job_list_calculate_level last cached. */
s32 job_list_calculate_next_level_jp(s32 index);

/* Row callbacks returning the values the calculators last cached in the globals above. */
s32 job_list_get_level(s32 unused_row);
s32 job_list_get_next_level_jp(s32 unused_row);
s32 job_list_get_jp(s32 unused_row);
s32 job_list_get_total_jp(s32 unused_row);
s32 job_list_is_mastered(s32 unused_row);

#endif
