#ifndef JOB_H
#define JOB_H

/* Jobs and abilities shared by the menu overlays and WORLD: job levels,
 * mastered skills and ability lists. */

#include "psx/types.h"

/* mastery and abilities */
s32 job_calculate_current_mastered_variant(s32 index);
/* What ability_build_list_variant lists: every ability of the job, the learned
 * ones, the learn menu's rows, or only a count of the unlearned ones. */
typedef enum ability_list_mode {
    ABILITY_LIST_MODE_VIEW_ALL = 0,
    ABILITY_LIST_MODE_LEARNED_ONLY = 1,
    ABILITY_LIST_MODE_LEARN_MENU = 2,
    ABILITY_LIST_MODE_COUNT_UNLEARNED = 3,
} ability_list_mode_e;

s32 ability_build_list_variant(s16 unit_index, s16 job_id, s32 ability_category, s16* ability_list, s32 mode);

/* state */
extern s32 g_job_current_mastered;
extern s16 g_job_selected_id;

s32 job_calculate_current_level_variant(s32 index);
s32 job_calculate_current_total_jp_variant(s32 index);
s32 job_get_base_variant(s16 unit_id);
s32 job_get_current_jp(s32 unused_row);
s32 job_get_current_total_jp(s32 unused_row);
s32 job_is_current_mastered(s32 unused_row);
extern u32 g_job_current_jp;
extern u32 g_job_current_total_jp;

s32 job_calculate_current_jp_variant(s32 index);

#endif
