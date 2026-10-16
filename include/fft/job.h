#ifndef JOB_H
#define JOB_H

/* Jobs and abilities shared by the menu overlays and WORLD: the selected job's level, JP and mastery, ability
 * lists, and the row callbacks of the job and ability menus. */

#include "fft/data_ability.h"
#include "psx/types.h"

/* job levels and JP */
/* The job highlighted in the job list, and the JP, total JP and mastery the job calculators last cached for the
 * row being drawn. */
extern s16 g_job_selected_id;
extern u32 g_job_current_jp;
extern u32 g_job_current_total_jp;
extern s32 g_job_current_mastered;

/* Row callbacks returning the cached values. */
s32 job_get_current_jp(s32 unused_row);
s32 job_get_current_total_jp(s32 unused_row);
s32 job_is_current_mastered(s32 unused_row);
/* Caches and returns whether the index-th job's skillset has nothing left to learn for the selected unit. */
s32 job_calculate_current_mastered(s32 index);

/* ability lists */
/* What ability_build_list lists: every ability of the job, the learned
 * ones, the learn menu's rows, or only a count of the unlearned ones. */
typedef enum ability_list_mode {
    ABILITY_LIST_MODE_VIEW_ALL = 0,
    ABILITY_LIST_MODE_LEARNED_ONLY = 1,
    ABILITY_LIST_MODE_LEARN_MENU = 2,
    ABILITY_LIST_MODE_COUNT_UNLEARNED = 3,
} ability_list_mode_e;

/* BUNIT and WORLD each build their own body. */
s32 ability_build_list(s16 unit_index, s16 job_id, s32 ability_category, s16* ability_list, s32 mode);

/* job and ability menus */
/* Step of the job list menus (WORLD learn and view-abilities menus, BUNIT job
 * menu, the JOBSTTS page); each list's handler returns the next step. In
 * JOBSTTS, JOBS is jobstts_menu_run_job_list, ABILITIES is
 * jobstts_menu_ability_update_list_menu and CLOSE makes jobstts_entrypoint close
 * the screen. */
typedef enum menu_job_list_step {
    MENU_JOB_LIST_STEP_CLOSE = -1,
    MENU_JOB_LIST_STEP_JOBS = 0,
    MENU_JOB_LIST_STEP_ABILITIES = 1,
} menu_job_list_step_e;

/* Ability list tab of the WORLD, BUNIT and JOBSTTS ability menus
 * (g_world_ability_category and g_menu_ability_category) and the kind
 * argument of their list builders. */
typedef enum menu_ability_category {
    MENU_ABILITY_CATEGORY_ACTION = 0,
    MENU_ABILITY_CATEGORY_REACTION = 1,
    MENU_ABILITY_CATEGORY_SUPPORT = 2,
    MENU_ABILITY_CATEGORY_MOVEMENT = 3,
    MENU_ABILITY_CATEGORY_ALL = 15, /* the list builders take any other value as all */
} menu_ability_category_e;

/* job menu callbacks */
/* Row callbacks of the job lists: the menu script calls them for a row's level and JP numbers. */
extern s16 g_menu_job_ids[];
extern s32 g_menu_job_current_level;
extern u32 g_menu_job_next_level_jp_requirement;

/* JP needed for the next level of the job whose level menu_job_get_current_level last cached. */
s32 menu_job_calculate_next_level_jp_requirement(s32 index);
s32 menu_job_get_current_level(s32 unused_row);
s32 menu_job_get_next_level_jp_requirement(s32 unused_row);

/* ability menu callbacks */
/* Row callbacks of the ability lists: predicates that pick a row's icon or color, and its displayed costs. The
 * lookups cache the listed ability's data for the panel. */
extern s16 g_menu_ability_category;  /* menu_ability_category_e; BUNIT and JOBSTTS tab selection */
extern s16 g_menu_ability_entries[]; /* Packed ability IDs/flags; -1 terminates the list. */
extern s32 g_menu_ability_type;
extern ability_data_t* g_menu_selected_ability_data;
extern main_ability_secondary_pointer_t g_menu_selected_ability_secondary_data;

/* 1 unless the entry's disabled bit (0x4000) is set. */
s32 menu_ability_is_learned(s32 index);
s32 menu_ability_is_action(s32 unused_row);
s32 menu_ability_is_non_action_learned(s32 index);
s32 menu_ability_is_non_action_unlearned(s32 index);
s32 menu_ability_is_action_tab(s32 unused_row);
s32 menu_ability_is_reaction_tab(s32 unused_row);
s32 menu_ability_is_support_tab(s32 unused_row);
s32 menu_ability_is_movement_tab(s32 unused_row);
s32 menu_ability_get_mp_cost_display_value(s32 index);
s32 menu_ability_get_ct_display_value(s32 index);
s32 menu_ability_get_jp_cost_display_value(s32 index);

#endif
