#ifndef MENU_COMMAND_H
#define MENU_COMMAND_H

/* BATTLE's command menus, built by the menu library: the battle menu ids, the turn menus, the skillset and
 * ability lists, action restrictions, how a selection resolves to a command, the system functions (messages
 * and confirmations) and auto-battle. */

#include "fft/menu.h"
#include "fft/menu_list.h"
#include "psx/types.h"

/* battle menu ids */
/* g_menu_current_id: the open battle menu,
 * and the index of its entry in g_menu_idle_action_entries, which is also what a row action from 0 to
 * 0xff opens. The unit menus are picked by
 * battle_menu_get_idle_action_menu_id. A system function
 * (menu_system_function_t) shows its text in MESSAGE or in one of the
 * choice entries and sets the current id itself; ids from 0x62 exist only
 * there and have no entry.
 *
 * Nothing in the C or in the two tables opens EMPTY_MESSAGE,
 * PERFORMING_MESSAGE, PROMPT_16, PROMPT_17 or the four NO_ACT turn menus,
 * and WAIT_YES_NO is linked only by a row action that is overwritten. */
typedef enum menu_command_id {
    MENU_COMMAND_ID_NONE = -1,
    MENU_COMMAND_ID_TURN = 0,                /* the unit's turn: it can move and act */
    MENU_COMMAND_ID_AUTO_BATTLE = 1,         /* the Auto-battle row: the five settings */
    MENU_COMMAND_ID_AUTO_BATTLE_TARGET = 2,  /* Change target, Check target: a setting chosen again */
    MENU_COMMAND_ID_ABILITY_LIST = 3,        /* the abilities or items of the chosen SKILLSET_LIST row */
    MENU_COMMAND_ID_SKILLSET_LIST = 4,       /* the Act row: the unit's commands */
    MENU_COMMAND_ID_STATUS = 5,              /* the Status row: the unit status panel */
    MENU_COMMAND_ID_WAIT = 6,                /* the Wait row */
    MENU_COMMAND_ID_WAIT_YES_NO = 7,         /* WAIT's row action in the table; menu_wait_thread replaces it */
    MENU_COMMAND_ID_STATUS_LIST = 8,         /* the status panel's list of current statuses */
    MENU_COMMAND_ID_EMPTY_MESSAGE = 9,       /* a message entry with empty text 0x8000 */
    MENU_COMMAND_ID_MINI_MENU = 10,          /* menu_start_mini_menu_display_thread: AT, Unit List, Option */
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
    MENU_COMMAND_ID_MOVE = 34,                       /* the Move row: menu_confirm_move_command_thread */
    MENU_COMMAND_ID_ACTION_RESTRICTION_MESSAGE = 35, /* why the unit cannot act or use the ability */
    MENU_COMMAND_ID_ABILITY_CONFIRM = 36,            /* an ABILITY_LIST row: menu_validate_skill_selection_thread */
    /* The same choices under MESSAGE. */
    MENU_COMMAND_ID_MESSAGE_TARGET_KIND = 37,
    MENU_COMMAND_ID_MESSAGE_PANEL_EXECUTE = 38,
    MENU_COMMAND_ID_AUTO_BATTLE_CONFIRM = 39, /* an AUTO_BATTLE row chosen for the first time */
    MENU_COMMAND_ID_AUTO_BATTLE_UNIT = 40,    /* Attack, Quit or Defend, Quit: the unit an auto-battle setting names */
    MENU_COMMAND_ID_MESSAGE_ATTACK_UNIT = 41, /* Attack, Quit under MESSAGE */
    MENU_COMMAND_ID_MESSAGE_DEFEND_UNIT = 42, /* Defend, Quit under MESSAGE */
    MENU_COMMAND_ID_EFFECT_MESSAGE = 43,      /* MENU_SYSTEM_FUNCTION_EFFECT_MESSAGE's entry */
    MENU_COMMAND_ID_DEBUGCHR = 44,            /* menu_open_debugchr */
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
    MENU_COMMAND_ID_DEAD_UNIT = 105,              /* crystal or treasure options */
} menu_command_id_e;

extern s32 g_menu_current_id; /* menu_command_id_e */

/* turn menus */
/* Per-skillset menu behavior (g_main_action_menu_types_by_skillset). Values select specialized
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
    ACTION_MENU_TYPE_COUNT = 16,
} action_menu_type_e;

/* Contextual skillset IDs synthesized by the action menu. The value 0xbc is
 * also an ordinary monster skillset when it comes from a monster unit. */
typedef enum action_menu_pseudo_skillset {
    ACTION_MENU_PSEUDO_SKILLSET_ANYTHING = 188,
} action_menu_pseudo_skillset_e;

/* The scene's menu table; BATTLE's is indexed by menu_command_id_e. */
extern menu_entry_t g_menu_idle_action_entries[];
extern s16 g_menu_wait_row_actions[4];

/* Opens an idle action menu (menu_command_id_e) for the active-turn unit beside it and restores the row it last
 * chose. WORLD's body only clears g_menu_building_thread_started. */
void menu_build_idle_action_menu(s32 unused_menu_id);
/* Thread entry of the turn menus: the icon selection loop, then the chosen row remembered for the unit. */
void menu_build_main(void);
/* Thread entry of the Wait menu. */
void menu_wait_thread(void);
/* Posts the close command after two frames under the mini-menu task id. */
void menu_mini_menu_cancel_delay_thread(void);
/* Queues the confirm sound and holds the mini-menu task id for 16 frames. */
void menu_confirm_sound_delay_thread(void);
/* Thread entry of the Move row: refuses a unit that cannot move, otherwise confirms the command. */
void menu_confirm_move_command_thread(void);

/* skillsets and abilities */
/* The ability list the skillset menus build in the event work block (event_work_t::ability_list): 82 rows of
 * parallel columns. ids, values and extras become the three text-id columns of g_menu_ability_layout.
 * Provisional. */
typedef struct menu_ability_list {
    u16 ids[82];     /* 0x000; 0xffff terminated; the text id base of the names is ORed in */
    u16 values[82];  /* 0x0a4 */
    u16 extras[82];  /* 0x148 */
    u8 mp_costs[82]; /* 0x1ec; 0xff terminated */
    u8 ct[82];       /* 0x23e; CT */
    u8 flags[82];    /* 0x290; bit 1 = blocked by unit flag 0x59 & 8 */
    u8 turns[82];    /* 0x2e2 */
    u8 skill_ct[82]; /* 0x334 */
} menu_ability_list_t;

/* Provisional: one 5-byte row of a menu text entry as rewritten by
 * menu_build_skillset_entries: the style byte at +1 is
 * 4 for a grayed row, the byte at +3 is the row separator (0xfa) or the
 * list terminator (0xff). */
typedef struct menu_text_row {
    u8 _unused_00;
    u8 style; /* 0x01 */
    u8 _unused_02;
    u8 terminator; /* 0x03: 0xfa continues, 0xff ends */
    u8 _unused_04;
} menu_text_row_t;

/* The mode argument of menu_build_ability_list: which list an Arithmeticks
 * (Math Skill) menu shows. Any other menu type builds its ability list. */
typedef enum menu_arithmeticks_list {
    MENU_ARITHMETICKS_LIST_ABILITIES = 0,  /* battle_menu_collect_calculator_abilities */
    MENU_ARITHMETICKS_LIST_MULTIPLES = 1,  /* battle_menu_load_math_skill_multiples */
    MENU_ARITHMETICKS_LIST_ATTRIBUTES = 2, /* battle_menu_load_math_skill_attributes */
} menu_arithmeticks_list_e;

extern u8 g_menu_skillsets[]; /* SKILLSET_ID_NONE terminates the rows. */
extern s16 g_menu_skillset_row_actions[];
/* -1: missing unit, 1: monster skillset, otherwise 0. */
extern s32 g_menu_skillset_build_result;
extern s32 g_menu_selected_skill_ct;
/* The ability list's layout; its menu entry passes it as the context. */
extern menu_scroll_text_layout_t g_menu_ability_layout;
extern u16 g_menu_ability_display_flags[80];
extern s16 g_menu_action_type_modes[][2];

/* Builds the skillset list, then waits for its submenu or runs the icon selection. */
void menu_run_skillset_thread(void);
/* BATTLE and WORLD each build their own body. */
void menu_run_scrolling_ability_list_thread(void);
/* Checks the chosen ability (MP, blocking statuses, per-menu restrictions) and shows why it cannot be used. */
void menu_validate_skill_selection_thread(void);

/* status list */
/* The status panel's list of the unit's current statuses, in two columns. */
extern menu_scroll_text_layout_t g_menu_status_list_layout;
extern u16 g_menu_status_list_row_actions[8];
extern s16 g_menu_status_left_text_ids[20];
extern s16 g_menu_status_right_text_ids[20];

void menu_build_unit_status_list(void);

/* action restrictions */
extern s16 g_menu_action_slot_row_actions[6];
extern s16 g_menu_action_slot_selected_option;
extern u8 g_menu_restriction_message_ids[];

/* Walks the stored unit's menu record slots from the top down to zero, presenting each and writing the chosen
 * entry back into the record. */
void menu_select_unit_action_slots_thread(void);
/* Shows the warning that applies when the unit cannot act from this menu. */
void menu_check_action_restrictions(void);
/* Checks a chosen row before it opens: deep water, Frog or the menu's own restriction shows its message and ends
 * the thread. */
void menu_check_action_slot_restrictions(s32 menu, s32 value, s32 check_unit);

/* command resolution */
/* One row of the 31-entry menu table at g_menu_command_maps; its first 12 rows are the
 * menus whose selection is remembered per unit.
 * menu_restore_selection_for_selected_unit and menu_record_selection_for_selected_unit
 * prove the 8-byte stride and read only the leading menu id;
 * menu_resolve_selection walks all 31 rows. */
typedef struct menu_command_map {
    u16 menu_id;
    u16 _padding_02; /* aligns map */
    /* 0x04: per-menu command remap table. map[0] handles the 0xFF selection
     * and map[n + 1] any other value; named by menu_resolve_selection,
     * its only reader. The other readers use menu_id alone. */
    u8* map;
} menu_command_map_t;
typedef char menu_command_map_size_must_be_8[(sizeof(menu_command_map_t) == 8) ? 1 : -1];

extern menu_command_map_t g_menu_command_maps[31];
extern s32 g_menu_active_menu; /* active menu */
extern s16 g_menu_result;      /* menu result: -4 pending, -1 canceled */
extern s16 g_menu_selected_option;
extern s32 g_menu_resolved_command;
extern s32 g_menu_used_skillset_id;
extern s32 g_menu_used_item_id;
extern u16 g_menu_anything_ability_id;

/* Publishes the resolved command, option and item and returns them packed into one value. */
s32 menu_resolve_selection(void);

/* system functions */
/* One row of g_menu_system_function_table (menu_run_system_function_thread,
 * menu_dispatch_system_function). Provisional. */
typedef struct menu_system_function {
    s16 menu_id;                /* 0x00; copied to g_menu_current_id */
    u16 text_id;                /* 0x02; menu entry text id */
    s16 menu_entry_index;       /* 0x04; g_menu_thread_menu_data index */
    u16 row_action;             /* 0x06; copied to g_menu_system_function_row_actions */
    u16 thread_count;           /* 0x08; menu entry thread_count */
    u8 _padding_0a[2];          /* aligns thread_entry */
    void (*thread_entry)(void); /* 0x0c; started as thread 8 */
    s16 alternate_id;           /* 0x10; replaces the id when navigation messages are not On */
    u8 _padding_12[2];          /* tail padding to 4-byte alignment */
} menu_system_function_t;
typedef char menu_system_function_size_must_be_20[(sizeof(menu_system_function_t) == 20) ? 1 : -1];

/* First argument of menu_init_system_function and
 * menu_dispatch_system_function; `option` selects the variant. */
typedef enum menu_system_command {
    MENU_SYSTEM_COMMAND_TARGETING_PROMPT = 1, /* prompt for the resolved menu command */
    MENU_SYSTEM_COMMAND_MOVE_FEEDBACK = 2,    /* option 0 closes, 1 and 4 confirm, others reject the move */
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
    MENU_SYSTEM_COMMAND_POST_ACTION_RESULT = 17, /* option: action_display_image_id_e */
} menu_system_command_e;

/* Result of menu_dispatch_system_function (and of
 * menu_init_system_function, which returns it). */
typedef enum menu_system_dispatch_result {
    MENU_SYSTEM_DISPATCH_UNKNOWN_COMMAND = 0, /* the CLOSE function was started instead */
    MENU_SYSTEM_DISPATCH_DONE = 1,            /* a system function was started, or nothing had to run */
    MENU_SYSTEM_DISPATCH_EVENT = 2,           /* SCENARIO_CHECK started an event, or queued it for option 2 */
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
    MENU_SYSTEM_FUNCTION_POST_ACTION_RESULT = 50,
} menu_system_function_id_e;

/* Array view: as a scalar, GCC hoists the menu-index reload above its store
 * in menu_run_system_function_thread. */
extern menu_system_function_t g_menu_system_function_table[];
extern s32 g_menu_system_function_thread_busy;
extern s32 g_menu_system_function_command;
extern s32 g_menu_system_function_option;
extern u16 g_menu_system_function_row_actions[];

/* Runs a system function (menu_system_command_e) for unit_id. */
s32 menu_init_system_function(s32 type, s32 option, s32 unit_id, s32 parameter, s32 enabled);

/* auto-battle */
extern s16 g_menu_auto_battle_option_colors[5]; /* text color per option */
extern s16 g_menu_set_auto_battle_row_actions;
extern s16 g_menu_confirm_stored_unit_row_actions;

/* The five-policy auto-battle menu. */
void menu_auto_battle_setting_thread(void);
/* Confirms the current row, recording the auto-battle mode it selects. */
void menu_confirm_auto_battle_selection_thread(void);
/* Stores a unit's auto-battle target with the resolved command as its setting. */
void menu_store_auto_battle_main_target_id(s32 target_id, s32 battle_id);

#endif
