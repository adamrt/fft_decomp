#ifndef COMMAND_H
#define COMMAND_H

/* The battle command menus of the event library: the turn menu (Move, Act, Wait), skillsets and ability lists,
 * command selection and per-unit remembered rows, Auto-Battle settings, the Triangle mini menu, system functions and
 * Status screen selection. */

#include "fft/ai_command.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* battle menu ids */
/* g_command_current_id: the open battle menu,
 * and the index of its entry in g_command_idle_action_entries, which is also what a row action from 0 to
 * 0xff opens. The unit menus are picked by
 * menu_action_get_menu_id. A system function
 * (menu_system_function_t) shows its text in MESSAGE or in one of the
 * choice entries and sets the current id itself; ids from 0x62 exist only
 * there and have no entry.
 *
 * Nothing in the C or in the two tables opens EMPTY_MESSAGE,
 * PERFORMING_MESSAGE, PROMPT_16, PROMPT_17 or the four NO_ACT turn menus,
 * and WAIT_YES_NO is linked only by a row action that is overwritten. */
typedef enum command_menu {
    MENU_COMMAND_ID_NONE = -1,
    MENU_COMMAND_ID_TURN = 0,                /* the unit's turn: it can move and act */
    MENU_COMMAND_ID_AUTO_BATTLE = 1,         /* the Auto-battle row: the five settings */
    MENU_COMMAND_ID_AUTO_BATTLE_TARGET = 2,  /* Change target, Check target: a setting chosen again */
    MENU_COMMAND_ID_ABILITY_LIST = 3,        /* the abilities or items of the chosen SKILLSET_LIST row */
    MENU_COMMAND_ID_SKILLSET_LIST = 4,       /* the Act row: the unit's commands */
    MENU_COMMAND_ID_STATUS = 5,              /* the Status row: the unit status panel */
    MENU_COMMAND_ID_WAIT = 6,                /* the Wait row */
    MENU_COMMAND_ID_WAIT_YES_NO = 7,         /* WAIT's row action in the table; command_wait_thread replaces it */
    MENU_COMMAND_ID_STATUS_LIST = 8,         /* the status panel's list of current statuses */
    MENU_COMMAND_ID_EMPTY_MESSAGE = 9,       /* a message entry with empty text 0x8000 */
    MENU_COMMAND_ID_MINI_MENU = 10,          /* mini_menu_start: AT, Unit List, Option */
    MENU_COMMAND_ID_OPTIONS = 11,            /* MINI_MENU's Option row: OPTION.OUT */
    MENU_COMMAND_ID_UNIT_LIST = 12,          /* MINI_MENU's Unit List row: BUNIT.OUT */
    MENU_COMMAND_ID_PERFORMING_MESSAGE = 13, /* a message entry with text 0x8002 */
    MENU_COMMAND_ID_TURN_MOVED = 14,         /* moved, or Don't Move */
    MENU_COMMAND_ID_PLAYER_UNIT = 15,        /* a player unit, not its turn */
    MENU_COMMAND_ID_OTHER_UNIT = 16,         /* any other unit, not its turn */
    /* The same two while the active unit is not under the player's control,
     * or while the status menu is open. */
    MENU_COMMAND_ID_PLAYER_UNIT_INSPECT = 17,
    MENU_COMMAND_ID_OTHER_UNIT_INSPECT = 18,
    MENU_COMMAND_ID_TURN_ACTED = 19, /* acted, or cannot act */
    MENU_COMMAND_ID_TURN_MOVED_ACTED = 20,
    MENU_COMMAND_ID_JOBSTTS = 21,   /* a STATUS_LIST row: JOBSTTS.OUT */
    MENU_COMMAND_ID_PROMPT_16 = 22, /* two equal message entries whose row opens PROMPT_YES_NO */
    MENU_COMMAND_ID_PROMPT_17 = 23,
    MENU_COMMAND_ID_PROMPT_YES_NO = 24, /* Yes, No */
    /* Yes, No under MESSAGE. As the current id: a system function that asks
     * (ride the chocobo, move here, inherit the crystal, learn the ability). */
    MENU_COMMAND_ID_MESSAGE_YES_NO = 25,
    /* The choices a system function shows without its navigation message. */
    MENU_COMMAND_ID_TARGET_KIND = 26,   /* Unit, Panel, Quit */
    MENU_COMMAND_ID_PANEL_EXECUTE = 27, /* Execute, Quit for a panel target */
    MENU_COMMAND_ID_MESSAGE = 28,       /* a system function's navigation message */
    MENU_COMMAND_ID_AT_LIST = 29,       /* MINI_MENU's AT row: OPTION.OUT's AT list */
    MENU_COMMAND_ID_EXECUTE = 30,       /* Execute, Quit */
    MENU_COMMAND_ID_ANNOUNCEMENT = 31,  /* the timed banner: an ability name or an action result */
    /* TURN and TURN_MOVED with the rows Move, Wait and Status only. */
    MENU_COMMAND_ID_TURN_NO_ACT = 32,
    MENU_COMMAND_ID_TURN_NO_ACT_MOVED = 33,
    MENU_COMMAND_ID_MOVE = 34,                       /* the Move row: command_move_thread */
    MENU_COMMAND_ID_ACTION_RESTRICTION_MESSAGE = 35, /* why the unit cannot act or use the ability */
    MENU_COMMAND_ID_ABILITY_CONFIRM = 36,            /* an ABILITY_LIST row: command_validate_ability_thread */
    /* The same choices under MESSAGE. */
    MENU_COMMAND_ID_MESSAGE_TARGET_KIND = 37,
    MENU_COMMAND_ID_MESSAGE_PANEL_EXECUTE = 38,
    MENU_COMMAND_ID_AUTO_BATTLE_CONFIRM = 39, /* an AUTO_BATTLE row chosen for the first time */
    MENU_COMMAND_ID_AUTO_BATTLE_UNIT = 40,    /* Attack, Quit or Defend, Quit: the unit an auto-battle setting names */
    MENU_COMMAND_ID_MESSAGE_ATTACK_UNIT = 41, /* Attack, Quit under MESSAGE */
    MENU_COMMAND_ID_MESSAGE_DEFEND_UNIT = 42, /* Defend, Quit under MESSAGE */
    MENU_COMMAND_ID_EFFECT_MESSAGE = 43,      /* MENU_SYSTEM_FUNCTION_EFFECT_MESSAGE's entry */
    MENU_COMMAND_ID_DEBUG_UNIT_EDITOR = 44,   /* debug_unit_editor_open */
    MENU_COMMAND_ID_MESSAGE_EXECUTE = 45,     /* Execute, Quit under MESSAGE */
    MENU_COMMAND_ID_RIDER_TURN = 46,          /* the TURN menus of a mounted unit, with a Stop ride row */
    MENU_COMMAND_ID_RIDER_TURN_MOVED = 47,
    MENU_COMMAND_ID_RIDER_TURN_ACTED = 48,
    MENU_COMMAND_ID_RIDER_TURN_MOVED_ACTED = 49,
    MENU_COMMAND_ID_RIDER_TURN_NO_ACT = 50,
    MENU_COMMAND_ID_RIDER_TURN_NO_ACT_MOVED = 51,
    MENU_COMMAND_ID_ABILITY_PREVIEW_AT_LIST = 52, /* the turn order with the ability under the cursor in it */
    MENU_COMMAND_ID_ABILITY_PREVIEW_MESSAGE = 53, /* shown instead when that ability gets no turn */
    MENU_COMMAND_ID_EGG = 54,                     /* replaces OTHER_UNIT and OTHER_UNIT_INSPECT for an egg */
    MENU_COMMAND_ID_EGG_STATUS = 55,              /* EGG's Status row: only the invalid sound */
    MENU_COMMAND_ID_CRYSTAL_PICKUP = 105          /* crystal or treasure options */
} command_menu_e;

extern u8 g_menu_skillset_types[]; /* per-row skillset kind; 13 keeps the row active */

void menu_action_dispatch(void);
command_menu_e menu_action_get_menu_id(s32 battle_id);
void menu_action_open_active_unit(void);
void menu_action_open_non_active_unit(void);

/* turn menu */
extern s32 g_command_current_id; /* command_menu_e */

/* BATTLE and WORLD each define the 56 records for command_menu_e IDs 0 through 55. */
extern menu_entry_t g_command_idle_action_entries[56];

/* Opens an idle action menu (command_menu_e) for the active-turn unit beside it and restores the row it last
 * chose. WORLD's body only clears g_menu_building_thread_started. */
void command_open_menu(s32 menu_id);

/* Thread entry of the turn menus: the icon selection loop, then the chosen row remembered for the unit. */
void command_turn_menu_thread(void);

/* Thread entry of the Move row: refuses a unit that cannot move, otherwise confirms the command. */
void command_move_thread(void);

/* Thread entry of the Wait menu. */
void command_wait_thread(void);

/* act menu */
/* Per-skillset menu behavior (g_command_menu_types_by_skillset). Values select specialized
 * inventory, targeting, and ability-loading paths; they are not menu command
 * opcodes. */
typedef enum action_menu_type {
    ACTION_MENU_TYPE_DEFAULT = 0,
    ACTION_MENU_TYPE_ITEM_INVENTORY = 1,
    ACTION_MENU_TYPE_WEAPON_INVENTORY = 2,
    ACTION_MENU_TYPE_ARITHMETICKS = 3,
    ACTION_MENU_TYPE_ELEMENTS = 4,
    ACTION_MENU_TYPE_BLANK_05 = 5,
    ACTION_MENU_TYPE_MONSTER = 6,
    ACTION_MENU_TYPE_KATANA_INVENTORY = 7,
    ACTION_MENU_TYPE_ATTACK = 8,
    ACTION_MENU_TYPE_JUMP = 9,
    ACTION_MENU_TYPE_CHARGE = 10,
    ACTION_MENU_TYPE_DEFEND = 11,
    ACTION_MENU_TYPE_CHANGE_EQUIPMENT = 12,
    ACTION_MENU_TYPE_UNKNOWN_0D = 13,
    ACTION_MENU_TYPE_BLANK_0E = 14,
    ACTION_MENU_TYPE_UNKNOWN_0F = 15,
    ACTION_MENU_TYPE_COUNT = 16
} action_menu_type_e;

/* Contextual skillset IDs synthesized by the action menu. The value 0xbc is
 * also an ordinary monster skillset when it comes from a monster unit. */
typedef enum action_menu_pseudo_skillset { ACTION_MENU_PSEUDO_SKILLSET_ANYTHING = 188 } action_menu_pseudo_skillset_e;

extern u8 g_command_menu_types_by_skillset[0xe0];
extern u8 g_command_skillsets[]; /* SKILLSET_ID_NONE terminates the rows. */
extern s32 g_command_selected_skill_ct;
extern s16 g_command_action_type_modes[][2];

/* -1: missing unit, 1: monster skillset, otherwise 0. */
extern s32 g_command_skillset_build_result;

/* Builds the skillset list, then waits for its submenu or runs the icon selection. */
void command_act_menu_thread(void);

/* Checks the chosen ability (MP, blocking statuses, per-menu restrictions) and shows why it cannot be used. */
void command_validate_ability_thread(void);

/* ability lists */
/* Packed menu attributes; the source ability record uses different bit positions. */
typedef enum menu_ability_flag {
    MENU_ABILITY_FLAG_REFLECTABLE = 0x01,
    MENU_ABILITY_FLAG_AFFECTED_BY_SILENCE = 0x02
} menu_ability_flag_e;

/* The ability list the skillset menus build in the event work block (event_work_t::ability_list): 82 rows of
 * parallel columns. ids, values and extras become the three text-id columns of g_ability_list_ability_layout.
 * Provisional. */
typedef struct menu_ability_list {
    u16 ids[82];     /* 0x000; 0xffff terminated; the text id base of the names is ORed in */
    u16 values[82];  /* 0x0a4 */
    u16 extras[82];  /* 0x148 */
    u8 mp_costs[82]; /* 0x1ec; 0xff terminated */
    u8 ct[82];       /* 0x23e; CT */
    u8 flags[82];    /* 0x290; menu_ability_flag_e, with 0xff terminating the byte column */
    u8 turns[82];    /* 0x2e2 */
    u8 skill_ct[82]; /* 0x334 */
} menu_ability_list_t;

extern s32 g_menu_scrolling_list_depth;

extern s16 g_command_action_slot_row_actions[6];

/* Walks the stored unit's menu record slots from the top down to zero, presenting each and writing the chosen
 * entry back into the record. */
void command_ability_list_thread(void);

/* Shows the warning that applies when the unit cannot act from this menu. */
void command_check_restrictions(void);

/* Checks a chosen row before it opens: deep water, Frog or the menu's own restriction shows its message and ends
 * the thread. */
void command_check_row_restrictions(s32 menu, s32 value, s32 check_unit);

/* selected command */
/* One row of the 31-entry menu table at g_command_maps; its first 12 rows are the menus whose selection is
 * remembered per unit. command_restore_unit_selection and command_record_unit_selection prove the 8-byte stride and
 * read only the leading menu id; command_resolve_selection walks all 31 rows. */
typedef struct menu_command_map {
    u16 menu_id;
    u16 _padding_02; /* aligns map */
    /* 0x04: per-menu command remap table. map[0] handles the 0xFF selection
     * and map[n + 1] any other value; named by command_resolve_selection,
     * its only reader. The other readers use menu_id alone. */
    u8* map;
} menu_command_map_t;
typedef char menu_command_map_size_must_be_8[(sizeof(menu_command_map_t) == 8) ? 1 : -1];

extern menu_command_map_t g_command_maps[31];

/* The selected command: its kind, then the action staged for at_list_preview. */
extern ai_command_t g_command_selected_command;

ai_command_t* command_get_selected(void);

/* Publishes the resolved command, option and item and returns them packed into one value. */
s32 command_resolve_selection(void);

/* unit records */
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

/* system functions */
/* First argument of menu_init_system_function and
 * menu_dispatch_system_function; `option` selects the variant. */
typedef enum menu_system_command {
    MENU_SYSTEM_COMMAND_TARGETING_PROMPT = 1, /* prompt for the resolved menu command */
    MENU_SYSTEM_COMMAND_MOVE_FEEDBACK = 2,    /* option: menu_move_feedback_option_e */
    MENU_SYSTEM_COMMAND_WAIT_MENU = 3,
    MENU_SYSTEM_COMMAND_ACTION_CONFIRM = 4,
    MENU_SYSTEM_COMMAND_SPELL_QUOTE = 5,
    MENU_SYSTEM_COMMAND_CLOSE = 6,
    MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT = 7,
    MENU_SYSTEM_COMMAND_SCENARIO_CHECK = 8, /* may start event 0x190-0x193; otherwise closes */
    MENU_SYSTEM_COMMAND_TARGET_CONFIRM = 9,
    MENU_SYSTEM_COMMAND_EFFECT_MESSAGE = 10,  /* option: text id */
    MENU_SYSTEM_COMMAND_ABILITY_LEARNED = 11, /* option: ability id */
    MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE = 12,
    MENU_SYSTEM_COMMAND_MOVE_CONFIRM = 13,
    MENU_SYSTEM_COMMAND_TRAP_RESULT = 14,    /* option: trap id */
    MENU_SYSTEM_COMMAND_MOVE_FIND_ITEM = 15, /* option: item id */
    MENU_SYSTEM_COMMAND_MOVE_CONFIRM_SILENT = 16,
    MENU_SYSTEM_COMMAND_POST_ACTION_RESULT = 17 /* option: action_display_image_id_e */
} menu_system_command_e;

/* MENU_SYSTEM_COMMAND_TARGETING_PROMPT options. */
typedef enum menu_targeting_prompt_option {
    MENU_TARGETING_PROMPT_DEFAULT = 0,
    MENU_TARGETING_PROMPT_NO_TARGET = 1,    /* the ability preview marked no tile */
    MENU_TARGETING_PROMPT_NO_MOVE_RANGE = 2 /* the moving unit has no reachable tile */
} menu_targeting_prompt_option_e;

/* MENU_SYSTEM_COMMAND_MOVE_FEEDBACK options. Any option not named here rejects the move with the
 * invalid sound and system function option + 13. */
typedef enum menu_move_feedback_option {
    MENU_MOVE_FEEDBACK_CLOSE = 0,
    MENU_MOVE_FEEDBACK_MOUNT = 1,        /* confirm mounting the unit on the destination */
    MENU_MOVE_FEEDBACK_ILLEGAL_MOVE = 2, /* reject the chosen destination */
    MENU_MOVE_FEEDBACK_CRYSTAL = 4       /* confirm a step onto a crystal */
} menu_move_feedback_option_e;

/* MENU_SYSTEM_COMMAND_SCENARIO_CHECK options. */
typedef enum menu_scenario_check_option {
    MENU_SCENARIO_CHECK_START = 0, /* start the event the check finds */
    MENU_SCENARIO_CHECK_SKIP = 1,  /* skip the checks and close */
    MENU_SCENARIO_CHECK_TEST = 2   /* only record the event in g_script_next_event_id */
} menu_scenario_check_option_e;

/* Result of menu_dispatch_system_function (and of
 * menu_init_system_function, which returns it). */
typedef enum menu_system_dispatch_result {
    MENU_SYSTEM_DISPATCH_UNKNOWN_COMMAND = 0, /* the CLOSE function was started instead */
    MENU_SYSTEM_DISPATCH_DONE = 1,            /* a system function was started, or nothing had to run */
    MENU_SYSTEM_DISPATCH_EVENT = 2            /* SCENARIO_CHECK started an event, or queued it for option 2 */
} menu_system_dispatch_result_e;

/* Indices of g_menu_system_function_table that the dispatcher names; the rest are message variants. */
typedef enum menu_system_function_id {
    MENU_SYSTEM_FUNCTION_WAIT_MENU = 1,
    MENU_SYSTEM_FUNCTION_CLOSE = 8,
    MENU_SYSTEM_FUNCTION_ABILITY_ANNOUNCEMENT = 10,
    MENU_SYSTEM_FUNCTION_EFFECT_MESSAGE = 29,
    MENU_SYSTEM_FUNCTION_ABILITY_LEARNED = 30,
    MENU_SYSTEM_FUNCTION_CRYSTAL_OR_TREASURE = 31,
    MENU_SYSTEM_FUNCTION_MOVE_CONFIRM = 33,
    MENU_SYSTEM_FUNCTION_TRAP_RESULT_FIRST = 34,
    MENU_SYSTEM_FUNCTION_MOVE_FIND_ITEM_SOLD = 38,
    MENU_SYSTEM_FUNCTION_MOVE_FIND_ITEM = 39,
    MENU_SYSTEM_FUNCTION_MOVE_CONFIRM_SILENT = 40,
    MENU_SYSTEM_FUNCTION_ARITHMETICKS_ANNOUNCEMENT = 44,
    MENU_SYSTEM_FUNCTION_POST_ACTION_RESULT = 50
} menu_system_function_id_e;

extern s32 g_menu_system_function_thread_busy;

/* Runs a system function (menu_system_command_e) for unit_id. */
s32 menu_init_system_function(s32 type, s32 option, s32 unit_id, s32 parameter, s32 enabled);

/* status screen */
extern s32 g_menu_status_enabled;
extern s32 g_menu_status_requested;
extern s32 g_menu_status_screen_selected;
extern s32 g_menu_status_open;

void menu_status_open(void);
void menu_status_clear(void);
void menu_enter_status_screen_selection(void);
void menu_leave_status_screen_selection(void);

#endif
