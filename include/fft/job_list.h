#ifndef JOB_LIST_H
#define JOB_LIST_H

/* The job page that the Unit List, the Status screen and Formation show: building the list and its row values. */

#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

extern u8 g_job_list_list_initialized;
extern s16 g_job_list_selected_generic_index;
extern s8 g_job_list_menu_active;
extern s8 g_job_list_menu_phase; /* menu_job_list_step_e */
extern menu_instruction_t g_job_list_list_menu_script;
s32 job_list_run(void);
s32 job_list_get_base(s16 unit_id);
s32 job_list_calculate_level(s32 index);
s32 job_list_calculate_jp(s32 index);
s32 job_list_calculate_total_jp(s32 index);

/* The job highlighted in the job list, and the JP, total JP and mastery the job calculators last cached for the
 * row being drawn. */
extern s16 g_job_list_selected_id;

/* Row callbacks returning the cached values. */
s32 job_list_get_jp(s32 unused_row);

s32 job_list_get_total_jp(s32 unused_row);
s32 job_list_is_mastered(s32 unused_row);

/* Caches and returns whether the index-th job's skillset has nothing left to learn for the selected unit. */
s32 job_list_calculate_mastered(s32 index);

/* Row callbacks of the job lists: the menu script calls them for a row's level and JP numbers. */
extern s16 g_job_list_job_ids[];

extern s32 g_job_list_job_current_level;
extern u32 g_job_list_job_next_level_jp_requirement;

/* JP needed for the next level of the job whose level job_list_get_level last cached. */
s32 job_list_calculate_next_level_jp(s32 index);

s32 job_list_get_level(s32 unused_row);
s32 job_list_get_next_level_jp(s32 unused_row);
extern s32 g_job_list_list_menu_initialized;
extern menu_instruction_t g_job_list_list_render_commands;
s32 job_list_run(void);
s32 job_list_build(u8 unit_id, s16* job_ids, s32 unused);
s32 job_list_calculate_level(s32 index);
s32 job_list_calculate_jp(s32 index);
s32 job_list_calculate_total_jp(s32 index);
s32 job_list_calculate_mastered(s32 index);
s32 job_list_build(u8 unit_index, s16* jobs, s32 mode);
s32 job_list_get_base(s16 unit_id);
s32 job_list_calculate_level(s32 index);
s32 job_list_calculate_total_jp(s32 index);

#endif
