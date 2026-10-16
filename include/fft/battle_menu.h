#ifndef BATTLE_MENU_H
#define BATTLE_MENU_H

/* BATTLE's menus: the action menus and the command they select, the ability lists of each
 * skillset, the unit panels and the status screen. */

#include "fft/battle_ai_command.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_command.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* menu state */
extern s32 g_state_in_help;

/* action menus */
extern u8 g_menu_idle_action_window_x[];
/* The selected command: its kind, then the action command staged for
 * at_list_preview. Both scenes alias the action at +4 under its own name. */
extern ai_command_t g_command_selected_command;
extern action_command_t g_command_selected_action;
extern u8 g_menu_skillset_types[];             /* per-row skillset kind; 13 keeps the row active */
extern u8 g_command_skillset_disabled_flags[]; /* per-row grayed flag */

s32 skillset_build_menu_list(s32 unit_id, u8* entries, u8* empty, u8* menu_types);
void menu_action_dispatch(void);
command_menu_e menu_action_get_menu_id(s32 battle_id);
ai_command_t* command_get_selected(void);
void menu_action_open_active_unit(void);
void state_enter_free_cursor_help(void);
void state_enter_mini_menu(void);
void state_enter_action_help_menu(void);
void state_enter_illegal_move_menu(void);
void state_return_to_mini_menu(void);
void state_enter_mini_menu_help(void);
void state_enter_move_confirm_menu(void);
void menu_action_open_non_active_unit(void);
void state_enter_open_action_menus(void);

/* ability lists */
/* The CLUT the outermost scrolling ability list saves, derives four more from and blinks while it
 * runs. */
extern RECT g_menu_ability_list_clut_rect;
extern u8 g_skillset_math_sources[4];
extern s32 g_menu_scrolling_list_depth;

s32 skillset_load_charge_ability_ids(s32 unit_id, u8 skillset_id, s16* ability_ids);
s32 skillset_math_collect_calculator_abilities(s32 unit_id, u8 skillset, s16* out);
s32 skillset_collect_draw_out_katanas(s32 unit_id, u8 skillset, u8* out_item_ids);
s32 skillset_monster_collect_abilities(s32 unit_id, u8 skillset, s16* out, s32 unused, u8* flags_out);
s32 skillset_collect_throwable_items(s32 unit_id, u8 skillset, u8* out);
s32 skillset_load_item_abilities(s32 unit_id, s32 skillset, u8* out_item_ids);
s32 skillset_get_unit_ability_data(
    s32 unit_id, u8 skillset, s16* ability_ids, u8* mp_out, u8* ct_out, s32 unused, u8* flags_out, u8* turns_out);
s32 skillset_monster_count_supporters(const unit_stats_t* unit);
s32 skillset_load_charge_abilities(s32 unit_id, s32 skillset, s16* abilities, u8* ability_ct, u8* turns);
s32 skillset_load_draw_out_abilities(s32 unit_id, s32 skillset, u8* out_item_ids);
u16 skillset_get_elemental_ability(s32 unit_id, u8 skillset);
s32 skillset_collect_item_abilities(s32 unit_id, u8 skillset, u8* out_item_ids);
s32 skillset_math_load_attributes(s32 unit_id, s32 skillset, s16* out_ability_ids);
s32 skillset_math_load_multiples(s32 unit_id, s32 skillset, s16* out_ability_ids);
s32 skillset_load_throw_abilities(s32 unit_id, s32 skillset, u8* out_item_ids);

/* Shared with the per-program event interpreter and dialog thread. */
u8* menu_redraw_text_page_on_scroll_in_rect(menu_entry_t* entry, s32* row_offset, s32* redraw, s32 unused, RECT* area);

/* unit panels */
/* The final two entries of g_unit_view_attack_caster_stats_setup. */
extern menu_number_entry_t g_menu_projected_value_panel[2];
/* Live caster-panel image parameters followed by their defaults. */
extern image_load_parameters_t g_menu_attack_caster_quad_image_params[2][5];

/* status screen */
extern s32 g_menu_status_enabled;
extern s32 g_menu_status_requested;
extern s32 g_menu_status_screen_selected;
extern s32 g_menu_status_open;

void menu_status_clear(void);
void menu_enter_status_screen_selection(void);
void menu_leave_status_screen_selection(void);

#endif
