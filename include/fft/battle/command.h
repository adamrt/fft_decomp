#ifndef COMMAND_H
#define COMMAND_H

/* The battle command menus of the event library: the turn menu (Move, Act, Wait), skillsets and ability lists,
 * command selection and per-unit remembered rows, Auto-Battle settings, the Triangle mini menu, system functions and
 * Status screen selection. */

#include "fft/battle/turn.h"
#include "fft/menu/menu.h"
#include "psx/types.h"

/* battle menu ids */
/* g_command_current_id: the open battle menu,
 * and the index of its entry in g_command_idle_action_entries, which is also what a row action from 0 to
 * 0xff opens. The unit menus are picked by
 * command_get_menu_id_by_battle_unit_id. A system function
 * (request_entry_t) shows its text in MESSAGE or in one of the
 * choice entries and sets the current id itself; ids from 0x62 exist only
 * there and have no entry.
 *
 * Nothing in the C or in the two tables opens EMPTY_MESSAGE,
 * PERFORMING_MESSAGE, PROMPT_16, PROMPT_17 or the four NO_ACT turn menus,
 * and WAIT_YES_NO is linked only by a row action that is overwritten. */
typedef enum command_menu {
    COMMAND_MENU_NONE = -1,
    COMMAND_MENU_TURN = 0,                /* the unit's turn: it can move and act */
    COMMAND_MENU_AUTO_BATTLE = 1,         /* the Auto-battle row: the five settings */
    COMMAND_MENU_AUTO_BATTLE_TARGET = 2,  /* Change target, Check target: a setting chosen again */
    COMMAND_MENU_ABILITY_LIST = 3,        /* the abilities or items of the chosen SKILLSET_LIST row */
    COMMAND_MENU_SKILLSET_LIST = 4,       /* the Act row: the unit's commands */
    COMMAND_MENU_STATUS = 5,              /* the Status row: the unit status panel */
    COMMAND_MENU_WAIT = 6,                /* the Wait row */
    COMMAND_MENU_WAIT_YES_NO = 7,         /* WAIT's row action in the table; command_wait_thread replaces it */
    COMMAND_MENU_STATUS_LIST = 8,         /* the status panel's list of current statuses */
    COMMAND_MENU_EMPTY_MESSAGE = 9,       /* a message entry with empty text 0x8000 */
    COMMAND_MENU_MINI_MENU = 10,          /* command_start_mini_menu: AT, Unit List, Option */
    COMMAND_MENU_OPTIONS = 11,            /* MINI_MENU's Option row: OPTION.OUT */
    COMMAND_MENU_UNIT_LIST = 12,          /* MINI_MENU's Unit List row: BUNIT.OUT */
    COMMAND_MENU_PERFORMING_MESSAGE = 13, /* a message entry with text 0x8002 */
    COMMAND_MENU_TURN_MOVED = 14,         /* moved, or Don't Move */
    COMMAND_MENU_PLAYER_UNIT = 15,        /* a player unit, not its turn */
    COMMAND_MENU_OTHER_UNIT = 16,         /* any other unit, not its turn */
    /* The same two while the active unit is not under the player's control,
     * or while the status menu is open. */
    COMMAND_MENU_PLAYER_UNIT_INSPECT = 17,
    COMMAND_MENU_OTHER_UNIT_INSPECT = 18,
    COMMAND_MENU_TURN_ACTED = 19, /* acted, or cannot act */
    COMMAND_MENU_TURN_MOVED_ACTED = 20,
    COMMAND_MENU_JOBSTTS = 21,   /* a STATUS_LIST row: JOBSTTS.OUT */
    COMMAND_MENU_PROMPT_16 = 22, /* two equal message entries whose row opens PROMPT_YES_NO */
    COMMAND_MENU_PROMPT_17 = 23,
    COMMAND_MENU_PROMPT_YES_NO = 24, /* Yes, No */
    /* Yes, No under MESSAGE. As the current id: a system function that asks
     * (ride the chocobo, move here, inherit the crystal, learn the ability). */
    COMMAND_MENU_MESSAGE_YES_NO = 25,
    /* The choices a system function shows without its navigation message. */
    COMMAND_MENU_TARGET_KIND = 26,   /* Unit, Panel, Quit */
    COMMAND_MENU_PANEL_EXECUTE = 27, /* Execute, Quit for a panel target */
    COMMAND_MENU_MESSAGE = 28,       /* a system function's navigation message */
    COMMAND_MENU_AT_LIST = 29,       /* MINI_MENU's AT row: OPTION.OUT's AT list */
    COMMAND_MENU_EXECUTE = 30,       /* Execute, Quit */
    COMMAND_MENU_ANNOUNCEMENT = 31,  /* the timed banner: an ability name or an action result */
    /* TURN and TURN_MOVED with the rows Move, Wait and Status only. */
    COMMAND_MENU_TURN_NO_ACT = 32,
    COMMAND_MENU_TURN_NO_ACT_MOVED = 33,
    COMMAND_MENU_MOVE = 34,                       /* the Move row: command_move_thread */
    COMMAND_MENU_ACTION_RESTRICTION_MESSAGE = 35, /* why the unit cannot act or use the ability */
    COMMAND_MENU_ABILITY_CONFIRM = 36,            /* an ABILITY_LIST row: command_validate_ability_thread */
    /* The same choices under MESSAGE. */
    COMMAND_MENU_MESSAGE_TARGET_KIND = 37,
    COMMAND_MENU_MESSAGE_PANEL_EXECUTE = 38,
    COMMAND_MENU_AUTO_BATTLE_CONFIRM = 39, /* an AUTO_BATTLE row chosen for the first time */
    COMMAND_MENU_AUTO_BATTLE_UNIT = 40,    /* Attack, Quit or Defend, Quit: the unit an auto-battle setting names */
    COMMAND_MENU_MESSAGE_ATTACK_UNIT = 41, /* Attack, Quit under MESSAGE */
    COMMAND_MENU_MESSAGE_DEFEND_UNIT = 42, /* Defend, Quit under MESSAGE */
    COMMAND_MENU_EFFECT_MESSAGE = 43,      /* MENU_PROMPT_EFFECT_MESSAGE's entry */
    COMMAND_MENU_DEBUG_UNIT_EDITOR = 44,   /* debug_unit_editor_thread */
    COMMAND_MENU_MESSAGE_EXECUTE = 45,     /* Execute, Quit under MESSAGE */
    COMMAND_MENU_RIDER_TURN = 46,          /* the TURN menus of a mounted unit, with a Stop ride row */
    COMMAND_MENU_RIDER_TURN_MOVED = 47,
    COMMAND_MENU_RIDER_TURN_ACTED = 48,
    COMMAND_MENU_RIDER_TURN_MOVED_ACTED = 49,
    COMMAND_MENU_RIDER_TURN_NO_ACT = 50,
    COMMAND_MENU_RIDER_TURN_NO_ACT_MOVED = 51,
    COMMAND_MENU_ABILITY_PREVIEW_AT_LIST = 52, /* the turn order with the ability under the cursor in it */
    COMMAND_MENU_ABILITY_PREVIEW_MESSAGE = 53, /* shown instead when that ability gets no turn */
    COMMAND_MENU_EGG = 54,                     /* replaces OTHER_UNIT and OTHER_UNIT_INSPECT for an egg */
    COMMAND_MENU_EGG_STATUS = 55,              /* EGG's Status row: only the invalid sound */
    COMMAND_MENU_CRYSTAL_PICKUP = 105          /* crystal or treasure options */
} command_menu_e;

command_menu_e command_get_menu_id_by_battle_unit_id(s32 battle_unit_id);

/* turn menu */
extern s32 g_command_current_id; /* command_menu_e */

/* BATTLE and WORLD each define the 56 records for command_menu_e IDs 0 through 55. */
extern menu_entry_t g_command_idle_action_entries[56];

/* Opens an idle action menu (command_menu_e) for the active-turn unit beside it and restores the row it last
 * chose. WORLD's body only clears g_menu_building_thread_started. */
void command_open_menu(s32 menu_id);

/* act menu */
/* Per-skillset menu behavior (g_command_menu_types_by_skillset). Values select specialized
 * inventory, targeting, and ability-loading paths; they are not menu command
 * opcodes. */
typedef enum action_menu_type {
    ACTION_MENU_TYPE_DEFAULT = 0,
    ACTION_MENU_TYPE_ITEM_INVENTORY = 1,
    ACTION_MENU_TYPE_WEAPON_INVENTORY = 2,
    ACTION_MENU_TYPE_MATH_SKILL = 3,
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

/* How a skillset kind's command proceeds, indexed by g_menu_skillset_types. Provisional. */
typedef struct command_action_type_mode {
    s16 row_action; /* negative: confirm at once with this row action instead of opening a list */
    s16 list_kind;  /* 1: the list shows inventory items by id */
} command_action_type_mode_t;
typedef char command_action_type_mode_size_must_be_4[(sizeof(command_action_type_mode_t) == 4) ? 1 : -1];

extern u8 g_command_menu_types_by_skillset[0xe0];
extern u8 g_command_skillsets[];   /* SKILLSET_ID_NONE terminates the rows. */
extern u8 g_menu_skillset_types[]; /* per-row skillset kind; 13 keeps the row active */
extern s32 g_command_selected_skill_ct;
extern command_action_type_mode_t g_command_action_type_modes[];

/* -1: missing unit, 1: monster skillset, otherwise 0. */
extern s32 g_command_skillset_build_result;

/* Builds the skillset list, then waits for its submenu or runs the icon selection. */
void command_act_menu_thread(void);

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
void command_check_act_restrictions(void);

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
    /* 0x04: indexed selections use map[n + 1]. map[0] is the retained 0xff fallback, bypassed by
     * command_resolve_selection's outer 0xfe/0xff guard. Other directory readers use menu_id alone. */
    u8* map;
} menu_command_map_t;
typedef char menu_command_map_size_must_be_8[(sizeof(menu_command_map_t) == 8) ? 1 : -1];

extern menu_command_map_t g_command_maps[31];

/* The selected command: its kind, then the action staged for at_list_preview. */
extern turn_command_t g_command_selected_command;

turn_command_t* command_get_selected(void);

/* Publishes the resolved command, option and item and returns them packed into one value. */
s32 command_resolve_selection(void);

/* unit records */
extern s16 g_command_pending_selection[4];

void command_reset_unit_records(void);
void command_record_unit_selection(void);

/* Clears one unit's records, only for a unit under manual control. */
void command_reset_unit_record(s32 battle_unit_id);

/* Restores the selected unit's command row after Move or Act changes the available choices. */
void command_restore_unit_selection(s32 menu_id);

/* Auto-Battle */
/* Stores a unit's auto-battle target with the resolved command as its setting. */
void auto_battle_store_target(s32 target_id, s32 battle_unit_id);

/* mini menu */
void command_start_mini_menu(void);

/* Waits for the camera and the menu transition, then runs the Triangle mini menu. */
void command_mini_menu_thread(void);

/* Posts the close command after two frames under the mini-menu task id. */
void command_request_close_thread(void);

/* system functions */
/* First argument of command_dispatch_request and
 * dispatch_request; `option` selects the variant. */
typedef enum menu_request {
    MENU_REQUEST_TARGETING_PROMPT = 1, /* prompt for the resolved menu command */
    MENU_REQUEST_MOVE_FEEDBACK = 2,    /* option: menu_move_feedback_option_e */
    MENU_REQUEST_WAIT_MENU = 3,
    MENU_REQUEST_ACTION_CONFIRM = 4,
    MENU_REQUEST_SPELL_QUOTE = 5,
    MENU_REQUEST_CLOSE = 6,
    MENU_REQUEST_ABILITY_ANNOUNCEMENT = 7,
    MENU_REQUEST_SCENARIO_CHECK = 8, /* may start event 0x190-0x193; otherwise closes */
    MENU_REQUEST_TARGET_CONFIRM = 9,
    MENU_REQUEST_EFFECT_MESSAGE = 10,  /* option: text id */
    MENU_REQUEST_ABILITY_LEARNED = 11, /* option: ability id */
    MENU_REQUEST_CRYSTAL_OR_TREASURE = 12,
    MENU_REQUEST_MOVE_CONFIRM = 13,
    MENU_REQUEST_TRAP_RESULT = 14,    /* option: trap id */
    MENU_REQUEST_MOVE_FIND_ITEM = 15, /* option: item id */
    MENU_REQUEST_MOVE_CONFIRM_SILENT = 16,
    MENU_REQUEST_POST_ACTION_RESULT = 17 /* option: action_display_image_id_e */
} menu_request_e;

/* MENU_REQUEST_TARGETING_PROMPT options. */
typedef enum menu_targeting_prompt_option {
    MENU_TARGETING_PROMPT_DEFAULT = 0,
    MENU_TARGETING_PROMPT_NO_TARGET = 1,    /* the ability preview marked no tile */
    MENU_TARGETING_PROMPT_NO_MOVE_RANGE = 2 /* the moving unit has no reachable tile */
} menu_targeting_prompt_option_e;

/* MENU_REQUEST_MOVE_FEEDBACK options. Any option not named here rejects the move with the
 * invalid sound and system function option + 13. */
typedef enum menu_move_feedback_option {
    MENU_MOVE_FEEDBACK_CLOSE = 0,
    MENU_MOVE_FEEDBACK_MOUNT = 1,        /* confirm mounting the unit on the destination */
    MENU_MOVE_FEEDBACK_ILLEGAL_MOVE = 2, /* reject the chosen destination */
    MENU_MOVE_FEEDBACK_CRYSTAL = 4       /* confirm a step onto a crystal */
} menu_move_feedback_option_e;

/* MENU_REQUEST_SCENARIO_CHECK options. */
typedef enum menu_scenario_check_option {
    MENU_SCENARIO_CHECK_START = 0, /* start the event the check finds */
    MENU_SCENARIO_CHECK_SKIP = 1,  /* skip the checks and close */
    MENU_SCENARIO_CHECK_TEST = 2   /* only record the event in g_script_next_event_id */
} menu_scenario_check_option_e;

/* Result of dispatch_request (and of
 * command_dispatch_request, which returns it). */
typedef enum menu_request_result {
    MENU_REQUEST_RESULT_UNKNOWN_COMMAND = 0, /* the CLOSE function was started instead */
    MENU_REQUEST_RESULT_DONE = 1,            /* a system function was started, or nothing had to run */
    MENU_REQUEST_RESULT_EVENT = 2            /* SCENARIO_CHECK started an event, or queued it for option 2 */
} menu_request_result_e;

/* Rows of the system-function table (s_request_entries in lib/menu/command_prompts.c) that the dispatcher names;
 * the rest are message variants. */
typedef enum menu_prompt_id {
    MENU_PROMPT_WAIT_MENU = 1,
    MENU_PROMPT_CLOSE = 8,
    MENU_PROMPT_ABILITY_ANNOUNCEMENT = 10,
    MENU_PROMPT_EFFECT_MESSAGE = 29,
    MENU_PROMPT_ABILITY_LEARNED = 30,
    MENU_PROMPT_CRYSTAL_OR_TREASURE = 31,
    MENU_PROMPT_MOVE_CONFIRM = 33,
    MENU_PROMPT_TRAP_RESULT_FIRST = 34,
    MENU_PROMPT_MOVE_FIND_ITEM_SOLD = 38,
    MENU_PROMPT_MOVE_FIND_ITEM = 39,
    MENU_PROMPT_MOVE_CONFIRM_SILENT = 40,
    MENU_PROMPT_MATH_SKILL_ANNOUNCEMENT = 44,
    MENU_PROMPT_POST_ACTION_RESULT = 50
} menu_prompt_id_e;

extern s32 g_menu_request_busy;

/* Runs a system function (menu_request_e) for unit_id. */
s32 command_dispatch_request(s32 type, s32 option, s32 unit_id, s32 parameter, s32 enabled);

/* status screen */
extern s32 g_menu_status_enabled;
extern s32 g_menu_status_requested;
extern s32 g_menu_status_screen_selected;
extern s32 g_menu_status_open;

void command_mark_status_menu_open(void);
void command_clear_status_menu(void);

#endif
