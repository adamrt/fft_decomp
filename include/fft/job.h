#ifndef JOB_H
#define JOB_H

/* Jobs and abilities shared by the menu overlays and WORLD: job levels,
 * mastered skills and ability lists. */

#include "fft/menu.h"
#include "psx/types.h"

/* unified */
s32 job_calculate_current_mastered(s32 index);
s32 ability_build_list_variant(s16 unit_index, s16 job_id, s32 ability_category, s16* ability_list, s32 mode);
extern s32 g_job_current_mastered;

#endif
