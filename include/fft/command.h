#ifndef COMMAND_H
#define COMMAND_H

/* The battle command menus of the event library: the turn menu (Move, Act, Wait), the Act menu's skillsets and ability
 * lists with their restrictions, the selected command, the rows remembered per unit, Auto-Battle settings
 * (auto_battle_) and the Triangle mini menu (mini_menu_). */

#include "fft/ai_command.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "psx/types.h"

/* turn menu */
extern s32 g_command_current_id; /* command_menu_e */
extern s16 g_command_wait_row_actions[4];
extern s16 g_command_confirm_stored_unit_row_actions;

/* The scene's menu table; BATTLE's is indexed by command_menu_e. */
extern menu_entry_t g_command_idle_action_entries[];

/* Opens an idle action menu (command_menu_e) for the active-turn unit beside it and restores the row it last
 * chose. WORLD's body only clears g_menu_building_thread_started. */
void command_open_menu(s32 menu_id);

/* Thread entry of the turn menus: the icon selection loop, then the chosen row remembered for the unit. */
void command_turn_menu_thread(void);

/* Thread entry of the Move row: refuses a unit that cannot move, otherwise confirms the command. */
void command_move_thread(void);

/* Thread entry of the Wait menu. */
void command_wait_thread(void);

/* Act menu */
extern u8 g_command_menu_types_by_skillset[0xe0];
extern u8 g_command_skillsets[]; /* SKILLSET_ID_NONE terminates the rows. */
extern s16 g_command_skillset_row_actions[];
extern u8 g_command_skillset_disabled_flags[]; /* per-row grayed flag */
extern s32 g_command_selected_skill_ct;
extern s16 g_command_action_type_modes[][2];

/* -1: missing unit, 1: monster skillset, otherwise 0. */
extern s32 g_command_skillset_build_result;

/* Builds the skillset list, then waits for its submenu or runs the icon selection. */
void command_act_menu_thread(void);

/* Checks the chosen ability (MP, blocking statuses, per-menu restrictions) and shows why it cannot be used. */
void command_validate_ability_thread(void);

/* ability lists */
extern s16 g_command_action_slot_row_actions[6];
extern s16 g_command_action_slot_selected_option;
extern u8 g_command_restriction_message_ids[];

/* Walks the stored unit's menu record slots from the top down to zero, presenting each and writing the chosen
 * entry back into the record. */
void command_ability_list_thread(void);

/* Shows the warning that applies when the unit cannot act from this menu. */
void command_check_restrictions(void);

/* Checks a chosen row before it opens: deep water, Frog or the menu's own restriction shows its message and ends
 * the thread. */
void command_check_row_restrictions(s32 menu, s32 value, s32 check_unit);

/* selected command */
extern menu_command_map_t g_command_maps[31];
extern s32 g_command_resolved_command;
extern s32 g_command_used_skillset_id;
extern s32 g_command_used_item_id;

/* The selected command: its kind, then the action command staged for
 * at_list_preview. Both scenes alias the action at +4 under its own name. */
extern ai_command_t g_command_selected_command;
extern action_command_t g_command_selected_action;

ai_command_t* command_get_selected(void);

/* Publishes the resolved command, option and item and returns them packed into one value. */
s32 command_resolve_selection(void);

/* unit records */
extern u8 g_command_restore_unit_id;
extern s16 g_command_pending_selection[4];

void command_reset_unit_records(void);
void command_record_unit_selection(void);

/* Clears one unit's records, only for a unit under manual control. */
void command_reset_unit_record(s32 battle_id);

/* Calls command_reset_unit_records; nothing calls it. */
void command_reset_unit_records_wrapper(void);

/* Restores the selected unit's command row, applying g_menu_selection_remaps. */
void command_restore_unit_selection(s32 menu_id);

/* Auto-Battle */
extern s16 g_auto_battle_option_colors[5]; /* text color per option */

/* The five-policy auto-battle menu. */
void auto_battle_settings_thread(void);

/* Confirms the current row, recording the auto-battle mode it selects. */
void auto_battle_confirm_thread(void);

/* Stores a unit's auto-battle target with the resolved command as its setting. */
void auto_battle_store_target(s32 target_id, s32 battle_id);

/* mini menu */
void mini_menu_start(void);

/* Waits for the camera and the menu transition, then runs the Triangle mini menu. */
void mini_menu_select(void);

/* Posts the close command after two frames under the mini-menu task id. */
void mini_menu_cancel_delay_thread(void);

/* Queues the confirm sound and holds the mini-menu task id for 16 frames. */
void mini_menu_confirm_delay_thread(void);

#endif
