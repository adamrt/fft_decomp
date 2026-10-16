#ifndef ABILITY_LIST_H
#define ABILITY_LIST_H

/* Ability lists: the scrolling ability list of the battle action menus, and the ability page that the Unit List, the
 * Status screen and Formation show, with its packed entries, list building and row values. */

#include "fft/ability.h"
#include "fft/job.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* scrolling ability list */
/* The ability list's layout; its menu entry passes it as the context. */
extern menu_scroll_text_layout_t g_ability_list_ability_layout;
extern u16 g_ability_list_ability_display_flags[80];
extern u16 g_ability_list_anything_ability_id;

/* BATTLE and WORLD each build their own body. */
void ability_list_scrolling_thread(void);

/* ability page */
/* Packed entries shared by the WORLD, BUNIT, and JOBSTTS ability menus. The
 * low-ten-bit mask is intentionally wider than ABILITY_ID_MASK: bit 9 is
 * unused by supported IDs, while bits 10 and 11 remain outside the ID. */
enum {
    ABILITY_LIST_ENTRY_ID_MASK = 0x03ff,
    ABILITY_LIST_ENTRY_DISABLED_SHIFT = 14,
    ABILITY_LIST_ENTRY_END = -1,
};

typedef enum ability_list_entry_flag {
    ABILITY_LIST_ENTRY_LEARNED = 0x1000,
    ABILITY_LIST_ENTRY_HIDE_DETAILS = 0x2000,
    ABILITY_LIST_ENTRY_DISABLED = 0x4000,
} ability_list_entry_flag_e;

extern u8 g_ability_list_list_ready;
extern u8 g_ability_list_list_initialized;
extern s8 g_ability_list_list_confirm_prompt_state;
extern s16 g_ability_list_list_job_name_row[];
extern s16 g_ability_list_list_job_name_ids[];
extern menu_instruction_t g_ability_list_list_menu_script;
extern menu_instruction_t g_ability_list_list_render_commands;

s32 ability_list_update(void);

/* BUNIT, JOBSTTS and WORLD each build their own body. */
s32 ability_list_build(
    s16 unit_index, s16 job_id, menu_ability_category_e ability_category, s16* ability_list, s32 mode);
s32 ability_list_build(
    s16 unit_index, s16 job_id, menu_ability_category_e ability_category, s16* list, s32 unlearned_only);

/* row callbacks */
/* Row callbacks of the ability lists: predicates that pick a row's icon or color, and its displayed costs. The
 * lookups cache the listed ability's data for the panel. */
extern s16 g_ability_list_ability_category;  /* menu_ability_category_e; BUNIT and JOBSTTS tab selection */
extern s16 g_ability_list_ability_entries[]; /* Packed ability IDs/flags; -1 terminates the list. */
extern s32 g_ability_list_ability_type;
extern ability_data_t* g_ability_list_selected_ability_data;
extern ability_secondary_pointer_t g_ability_list_selected_ability_secondary_data;

s32 ability_list_is_learned(s32 index); /* 1 unless the entry's disabled bit (0x4000) is set */
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
