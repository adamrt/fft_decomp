#ifndef BATTLE_MENU_H
#define BATTLE_MENU_H

/* Battle menus, window resources and event-overlay menu interfaces. */

#include "fft/battle_ai.h"
#include "fft/menu.h"

/* menu state */
extern s32 g_battle_menu_help_opening;

void battle_menu_clear_status_menu_state(void);

/* input and selection */

/* action menus */
extern u8 g_battle_menu_idle_action_entry_labels[];
/* The selected command: its kind, then the action command staged for
 * action_preview_at_list. Both scenes alias the action at +4 under its own name. */
extern ai_command_t g_menu_selected_command;
extern action_command_t g_menu_selected_action;

s32 battle_menu_build_unit_action_menus(s32 unit_id, u8* entries, u8* empty, u8* menu_types);
void battle_menu_close_move_help(void);
void battle_menu_dispatch_idle_action_menu(void);
battle_menu_id_e battle_menu_get_id_based_on_mount_moveable_actable(s32 battle_id);
ai_command_t* menu_get_selected_command_address(void);
void battle_menu_open_active_unit_idle_action_menu(void);
void battle_menu_open_free_cursor_help(void);
void battle_menu_open_free_cursor_mini_menu(void);
void battle_menu_open_help(void);
void battle_menu_open_illegal_move_help(void);
void battle_menu_open_mini_menu(void);
void battle_menu_open_mini_menu_help(void);
void battle_menu_open_move_confirm(void);
void battle_menu_open_non_active_unit_idle_action_menu(void);
void battle_menu_set_next_script_action_menus(void);

/* abilities */
/* Ability-list window layout at 0x80166994. Column modes: 0 text, 1 number,
 * 2 hidden, 3 alternate number. The columns read the work buffer's u16 text
 * ids and values through the layout's s16 pointers. */
extern RECT g_battle_menu_ability_list_clut_rect;
extern u8 g_battle_menu_calculator_skillsets[4];
extern s32 g_battle_menu_scroll_list_depth;
extern u8 g_battle_menu_skillset_disabled_flags[]; /* per-row grayed flag */

s32 battle_menu_call_load_charge_skillset(s32 unit_id, u8 skillset_id, s16* ability_ids);
s32 battle_menu_collect_calculator_abilities(s32 unit_id, u8 skillset, s16* out);
s32 battle_menu_collect_draw_out_katanas(s32 unit_id, u8 skillset, u8* out_item_ids);
s32 battle_menu_collect_monster_skill_abilities(s32 unit_id, u8 skillset, s16* out, s32 unused, u8* flags_out);
s32 battle_menu_collect_throwable_items(s32 unit_id, u8 ability, u8* items);
s32 battle_menu_display_item_inventory_ability(s32 unit_id, s32 skillset, u8* out_item_ids);
s32 battle_menu_get_unit_skillset_ability_data(
    s32 unit_id, u8 skillset, s16* ability_ids, u8* mp_out, u8* ct_out, s32 unused, u8* flags_out, u8* turns_out);
s32 battle_menu_init_monster_skill_check(const unit_stats_t* unit);
s32 battle_menu_load_charge_skillset(s32 unit_id, s32 skillset, s16* abilities, u8* ability_ct, u8* turns);
s32 battle_menu_load_draw_out_abilities(s32 unit_id, s32 skillset, u8* out_item_ids);
u16 battle_menu_load_elemental_abilities(s32 skillset_slot, u8 unit_id);
s32 battle_menu_load_item_abilities(s32 unit_id, u8 skillset, u8* out_item_ids);
s32 battle_menu_load_math_skill_attributes(s32 unit_id, s32 skillset, s16* out_ability_ids);
s32 battle_menu_load_math_skill_multiples(s32 unit_id, s32 skillset, s16* out_ability_ids);
s32 battle_menu_load_throw_abilities(s32 unit_id, s32 skillset, u8* out_item_ids);

/* unit panels */
/* The final two entries of g_menu_attack_caster_stats_setup. */
extern menu_number_entry_t g_battle_menu_projected_value_panel[2];

/* status screen */
extern s32 g_battle_menu_status_enabled;
extern s32 g_battle_menu_status_requested;
extern s32 g_battle_menu_status_screen_selected;

void battle_menu_enter_status_screen_selection(void);
void battle_menu_leave_status_screen_selection(void);

/* windows and sprites */
/* Live caster-panel image parameters followed by their defaults. */
extern image_load_parameters_t g_battle_menu_attack_caster_quad_image_params[2][5];

/* text rendering */
extern s32 g_battle_menu_text_section_offsets[];

/* overlay loading */

/* threads */

#endif
