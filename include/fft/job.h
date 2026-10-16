#ifndef JOB_H
#define JOB_H

/* Jobs and abilities shared by the menu overlays and WORLD: the selected job's level, JP and mastery, ability
 * lists, and the row callbacks of the job and ability menus. */

#include "fft/data_ability.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "psx/types.h"

/* job lookups */
/* The menu library's job helpers (src/event/menu), compiled into WORLD and the
 * EVENT overlays. */
void job_calculate_and_store_unlocks(party_unit_t* party, u8 unlocked_jobs[UNIT_UNLOCKED_JOB_BYTE_COUNT]);
s32 job_find_first_for_skillset(s32 skillset);
s32 job_get_generic_index(s32 job_id);
s32 job_get_skillset(s32 job_id);
s32 job_is_special_monster(s32 job_id);

/* job levels and JP */
/* The job highlighted in the job list, and the JP, total JP and mastery the job calculators last cached for the
 * row being drawn. */
extern s16 g_job_list_selected_id;
extern u32 g_job_current_jp;
extern u32 g_job_current_total_jp;
extern s32 g_job_current_mastered;

/* Row callbacks returning the cached values. */
s32 job_list_get_jp(s32 unused_row);
s32 job_list_get_total_jp(s32 unused_row);
s32 job_list_is_mastered(s32 unused_row);
/* Caches and returns whether the index-th job's skillset has nothing left to learn for the selected unit. */
s32 job_list_calculate_mastered(s32 index);

/* ability lists */
/* What ability_list_build lists: every ability of the job, the learned
 * ones, the learn menu's rows, or only a count of the unlearned ones. */
typedef enum ability_list_mode {
    ABILITY_LIST_MODE_VIEW_ALL = 0,
    ABILITY_LIST_MODE_LEARNED_ONLY = 1,
    ABILITY_LIST_MODE_LEARN_MENU = 2,
    ABILITY_LIST_MODE_COUNT_UNLEARNED = 3,
} ability_list_mode_e;

/* BUNIT and WORLD each build their own body. */

/* job and ability menus */
/* Step of the job list menus (WORLD learn and view-abilities menus, BUNIT job
 * menu, the JOBSTTS page); each list's handler returns the next step. In
 * JOBSTTS, JOBS is job_list_run, ABILITIES is
 * ability_list_update and CLOSE makes status_screen_entrypoint close
 * the screen. */
typedef enum menu_job_list_step {
    MENU_JOB_LIST_STEP_CLOSE = -1,
    MENU_JOB_LIST_STEP_JOBS = 0,
    MENU_JOB_LIST_STEP_ABILITIES = 1,
} menu_job_list_step_e;

/* Ability list tab of the WORLD, BUNIT and JOBSTTS ability menus
 * (g_formation_ability_category and g_ability_list_ability_category) and the kind
 * argument of their list builders. */
typedef enum menu_ability_category {
    MENU_ABILITY_CATEGORY_ACTION = 0,
    MENU_ABILITY_CATEGORY_REACTION = 1,
    MENU_ABILITY_CATEGORY_SUPPORT = 2,
    MENU_ABILITY_CATEGORY_MOVEMENT = 3,
    MENU_ABILITY_CATEGORY_ALL = 15, /* the list builders take any other value as all */
} menu_ability_category_e;

s32 ability_list_build(
    s16 unit_index, s16 job_id, menu_ability_category_e ability_category, s16* ability_list, s32 mode);

/* job menu callbacks */
/* Row callbacks of the job lists: the menu script calls them for a row's level and JP numbers. */
extern s16 g_job_list_job_ids[];
extern s32 g_job_list_job_current_level;
extern u32 g_job_list_job_next_level_jp_requirement;

/* JP needed for the next level of the job whose level job_list_get_level last cached. */
s32 job_list_calculate_next_level_jp(s32 index);
s32 job_list_get_level(s32 unused_row);
s32 job_list_get_next_level_jp(s32 unused_row);

/* ability menu callbacks */
/* Row callbacks of the ability lists: predicates that pick a row's icon or color, and its displayed costs. The
 * lookups cache the listed ability's data for the panel. */
extern s16 g_ability_list_ability_category;  /* menu_ability_category_e; BUNIT and JOBSTTS tab selection */
extern s16 g_ability_list_ability_entries[]; /* Packed ability IDs/flags; -1 terminates the list. */
extern s32 g_ability_list_ability_type;
extern ability_data_t* g_ability_list_selected_ability_data;
extern ability_secondary_pointer_t g_ability_list_selected_ability_secondary_data;

/* 1 unless the entry's disabled bit (0x4000) is set. */
s32 ability_list_is_learned(s32 index);
s32 ability_list_is_action(s32 unused_row);
s32 ability_list_is_non_action_learned(s32 index);
s32 ability_list_is_non_action_unlearned(s32 index);
s32 ability_list_is_action_tab(s32 unused_row);
s32 ability_list_is_reaction_tab(s32 unused_row);
s32 ability_list_is_support_tab(s32 unused_row);
s32 ability_list_is_movement_tab(s32 unused_row);
s32 ability_list_get_mp_cost(s32 index);
s32 ability_list_get_ct(s32 index);
s32 ability_list_get_jp_cost(s32 index);

#endif
