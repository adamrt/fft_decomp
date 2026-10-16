#ifndef FORMATION_ABILITY_H
#define FORMATION_ABILITY_H

/* The Formation screen's ability menu in WORLD: a unit's ability slots and checks, the Learn Abilities job and ability
 * lists with their view-only copy, and the Set and Remove ability steps. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
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
extern s16 g_formation_ability_category; /* menu_ability_category_e */
extern menu_entry_t g_formation_ability_menu;
extern s8 g_formation_ability_menu_initialized;
extern u8 g_formation_ability_menu_unit_index;
extern u8 g_formation_ability_menu_slots_changed;
extern u8 g_formation_ability_menu_saved_browse_enabled;
extern u8 g_formation_ability_submenu_running;
extern RECT g_formation_ability_panel_origin;
extern unit_panel_thread_t g_formation_ability_panel_thread_params;
extern menu_instruction_t g_formation_ability_list_script;
extern menu_list_page_t g_formation_ability_list_pages[][2];

void formation_ability_menu(void);

/* learning */
extern s8 g_formation_ability_learn_menu_initialized;
extern s8 g_formation_ability_learn_menu_step; /* menu_job_list_step_e */
extern s8 g_formation_ability_learn_job_list_initialized;
extern menu_instruction_t g_formation_ability_learn_job_list_script;
extern s16 g_formation_ability_learn_job_name_list[];
extern s32 g_formation_ability_learn_job_total_jp;
extern s8 g_formation_ability_learn_ability_list_open;
extern s16 g_formation_ability_learn_selected_job;
extern s16 g_formation_ability_learn_selected_job_index;
extern menu_entry_t g_formation_ability_learn_confirm_menu[2];

/* The prompt chained after g_formation_ability_learn_confirm_menu. */
extern menu_entry_t g_formation_ability_learn_confirm_submenu;

/* Nonzero while the learn-confirmation menu thread (thread 6) is running. */
extern u8 g_formation_ability_learn_confirm_prompt_state;

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
extern s8 g_formation_ability_view_step; /* menu_job_list_step_e */
extern u8 g_formation_ability_view_confirm_prompt_state;
extern s16 g_formation_ability_view_job_names[];
extern s8 g_formation_ability_view_list_initialized;

/* set ability */
extern s8 g_formation_ability_set_initialized;
extern u8 g_formation_ability_set_unit_index;
extern menu_instruction_t g_formation_ability_set_window_script;
extern s8 g_formation_ability_set_slot; /* ability slot under the cursor: 0..4 */
extern u8 g_formation_ability_set_cursor;
extern u16 g_formation_ability_set_cursor_anim;
extern point16_t g_formation_ability_set_cursor_point;
extern s8 g_formation_ability_set_list_open;          /* ability list open */
extern s8 g_formation_ability_set_entry_count;        /* ability list entry count */
extern s8 g_formation_ability_set_list_close_pending; /* close the ability list this frame */
extern s8 g_formation_ability_set_show_stat_preview;  /* show the stat preview window */
extern s8 g_formation_ability_set_unread_flag;        /* cleared when Set Ability opens; never read */

/* remove ability */
extern s8 g_formation_ability_remove_initialized;
extern s8 g_formation_ability_remove_unit_index;
extern s8 g_formation_ability_remove_cursor;
extern u16 g_formation_ability_remove_cursor_anim;
extern point16_t g_formation_ability_remove_cursor_point;
extern s8 g_formation_ability_remove_all_prompt_active;
extern volatile u16 g_formation_ability_remove_all_blink_counter;
extern point16_t g_formation_ability_remove_all_cursor_point;

#endif
