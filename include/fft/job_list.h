#ifndef JOB_LIST_H
#define JOB_LIST_H

/* The job page that the Unit List, the Status screen and Formation show: building the list and its row values. */

#include "fft/menu_script.h"
#include "psx/types.h"

/* job list */
extern u8 g_job_list_list_initialized;
extern s32 g_job_list_list_menu_initialized;
extern s8 g_job_list_menu_active;
extern s8 g_job_list_menu_phase; /* menu_job_list_step_e */
extern menu_instruction_t g_job_list_list_menu_script;
extern menu_instruction_t g_job_list_list_render_commands;
extern s16 g_job_list_selected_generic_index;

/* The job highlighted in the job list. */
extern s16 g_job_list_selected_id;

s32 job_list_run(void);
s32 job_list_get_base(s16 unit_id);

/* WORLD and JOBSTTS each build their own body. */
s32 job_list_build(u8 unit_id, s16* job_ids, s32 unused);
s32 job_list_build(u8 unit_index, s16* jobs, s32 mode);

/* row callbacks */
/* Row callbacks of the job lists: the menu script calls them for a row's level and JP numbers. */
extern s16 g_job_list_job_ids[];
extern s32 g_job_list_job_current_level;
extern u32 g_job_list_job_next_level_jp_requirement;

s32 job_list_calculate_level(s32 index);
s32 job_list_calculate_jp(s32 index);
s32 job_list_calculate_total_jp(s32 index);

/* Caches and returns whether the index-th job's skillset has nothing left to learn for the selected unit. */
s32 job_list_calculate_mastered(s32 index);

/* JP needed for the next level of the job whose level job_list_calculate_level last cached. */
s32 job_list_calculate_next_level_jp(s32 index);

/* Row callbacks returning the values the calculators last cached, in the globals above and in g_job_current_jp,
 * g_job_current_total_jp and g_job_current_mastered. */
s32 job_list_get_level(s32 unused_row);
s32 job_list_get_next_level_jp(s32 unused_row);
s32 job_list_get_jp(s32 unused_row);
s32 job_list_get_total_jp(s32 unused_row);
s32 job_list_is_mastered(s32 unused_row);

#endif
