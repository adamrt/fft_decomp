#ifndef FORMATION_ABILITY_H
#define FORMATION_ABILITY_H

/* The Formation screen's ability menu in WORLD: a unit's ability slots and checks, the Learn Abilities job and ability
 * lists with their view-only copy, and the Set and Remove ability steps. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_list.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* ability slots */
/* Formation ability slots are selected by index in the equipment menu. */
typedef struct formation_ability_fields {
    u16 primary_skillset;   /* 0x00 */
    u16 secondary_skillset; /* 0x02 */
    s16 reaction_ability;   /* 0x04 */
    u16 support_ability;    /* 0x06 */
    s16 movement_ability;   /* 0x08 */
} formation_ability_fields_t;

s32 formation_ability_set_slot(s16 unit_id, s16 slot, s16 ability, s32 update_equipment);
s32 formation_ability_build_rsm_list(s16 unit_index, s32 slot, s16* out);
void formation_ability_diff_movement_stats(unit_panel_record_t* stat_diff, s32 old_ability, s32 new_ability);
s32 formation_ability_has_secret_hunt(s32 unit_id);
s32 formation_ability_has_two_hands(s16 unit_id);

/* ability menu */
/* Command texts chosen by formation_unit_lock_e and whether the unit is a Mime. */
enum {
    FORMATION_ABILITY_MENU_TEXT_UNLOCKED = 0xd80b,
    FORMATION_ABILITY_MENU_TEXT_LOCKED = 0xd80c,
    FORMATION_ABILITY_MENU_TEXT_MIME = 0xd80d
};

extern s8 g_formation_ability_menu_initialized;
extern unit_panel_thread_t g_formation_ability_panel_thread_params;

void formation_ability_menu(void);

/* learning */
extern s8 g_formation_ability_learn_menu_initialized;
extern s8 g_formation_ability_learn_job_list_initialized;
extern s32 g_formation_ability_learn_job_total_jp;
extern s8 g_formation_ability_learn_ability_list_open;
extern s16 g_formation_ability_learn_selected_job;
extern s16 g_formation_ability_learn_selected_job_index;

s32 formation_ability_learn_job_list(void);
s32 formation_ability_learn_job_jp(s32 index);
s32 formation_ability_learn_list(void);
void formation_ability_learn(s16 unit_id, s16 job_id, s16 ability_id);

/* Row callbacks of the learn and view job lists: the level and JP values of the job chosen in the learn menu; the
 * row is ignored. */
s32 formation_ability_get_chosen_job_level(s32 unused_row);
s32 formation_ability_get_chosen_job_next_jp(s32 unused_row);
s32 formation_ability_is_chosen_job_mastered(s32 unused_row);
s32 formation_ability_get_chosen_job_total_jp(s32 unused_row);
s32 formation_ability_get_chosen_job_jp(s32 unused_row);

/* ability view */
extern s8 g_formation_ability_view_initialized;
extern s8 g_formation_ability_view_list_initialized;

/* set ability */
extern s8 g_formation_ability_set_initialized;

/* remove ability */
extern s8 g_formation_ability_remove_initialized;

#endif
