#ifndef MENU_HELP_H
#define MENU_HELP_H

/* Help: the help text sections, the cursor-navigation nodes the help cursor moves over, and the help screens of
 * the battle menus and REQUIRE. */

#include "fft/map.h"
#include "fft/menu_status.h"
#include "fft/text.h"
#include "psx/types.h"

/* help text */
/* The help text file, which HELPMENU and WORLD's help menu bind section by
 * section while a help window is open. */
typedef enum help_text_section {
    HELP_TEXT_SECTION_UNIT_STATUS = 1,   /* the fields of the unit status panels */
    HELP_TEXT_SECTION_MENU_COMMANDS = 2, /* the rows of each command menu */
    HELP_TEXT_SECTION_TERRAIN = 11,      /* by the cursor tile's surface type */
    HELP_TEXT_SECTION_JOBS = 12,         /* by job id */
    HELP_TEXT_SECTION_ITEMS = 13,        /* by item id */
    HELP_TEXT_SECTION_ABILITIES = 15,    /* by ability id */
    HELP_TEXT_SECTION_SKILLSETS = 19,    /* by skillset id */
    HELP_TEXT_SECTION_STATUSES = 20,     /* by status id */
} help_text_section_e;

/* The first text id of the help text sections the code adds an id to. help_navigation_record_t.help_base
 * holds the 16-bit form, in which the skillsets' base reads as -0x6800. */
enum {
    HELP_TEXT_ID_MENU_COMMAND_BASE = HELP_TEXT_SECTION_MENU_COMMANDS << TEXT_ID_SECTION_SHIFT,
    HELP_TEXT_ID_TERRAIN_BASE = HELP_TEXT_SECTION_TERRAIN << TEXT_ID_SECTION_SHIFT,
    HELP_TEXT_ID_JOB_BASE = HELP_TEXT_SECTION_JOBS << TEXT_ID_SECTION_SHIFT,
    HELP_TEXT_ID_ITEM_BASE = HELP_TEXT_SECTION_ITEMS << TEXT_ID_SECTION_SHIFT,
    HELP_TEXT_ID_ABILITY_BASE = HELP_TEXT_SECTION_ABILITIES << TEXT_ID_SECTION_SHIFT,
    HELP_TEXT_ID_SKILLSET_BASE = HELP_TEXT_SECTION_SKILLSETS << TEXT_ID_SECTION_SHIFT,
    HELP_TEXT_ID_STATUS_BASE = HELP_TEXT_SECTION_STATUSES << TEXT_ID_SECTION_SHIFT,
};

/* The dialog type of a help window: a Help box without a tail; the help screens center it. */
enum {
    HELP_DIALOG_TYPE = DIALOG_BOX_TYPE_HELP | DIALOG_OPTIONS_NO_TAIL,
    HELP_DIALOG_TYPE_CENTERED = HELP_DIALOG_TYPE | DIALOG_POSITION_CENTER,
};

/* The help text file's section offsets, which text_relocate_pointer_table binds while a help window is open. */
extern u32 g_help_text_table[];
extern u16* const* g_menu_help_active_text_id_tables;
extern u16* const g_menu_help_menu_text_id_tables[23];
extern u16* g_menu_help_topic_text_ids;
/* The pending help message, or a menu_grid_help_id_e. */
extern s32 g_menu_help_message_text_id;

/* help navigation */
/* One cursor-navigation node of the HELPMENU, BUNIT, ATTACK deployment and
 * DEBUGCHR editor screens (WORLD's help_navigation_record_t has the same
 * layout): the value or text id it refers to, its help-text base, the node
 * reached for each d-pad direction (up 0x1000, down 0x4000, right 0x2000,
 * left 0x8000) and the cursor position. */
typedef struct help_navigation_record {
    s16* value;          /* 0x00 */
    s16 help_base;       /* 0x04 */
    u8 destination[4];   /* 0x06: up, down, right, left */
    s16 x;               /* 0x0a */
    s16 y;               /* 0x0c */
    s16 vertical_cursor; /* 0x0e: nonzero: the cursor points down and bobs along y */
} help_navigation_record_t;
typedef char help_navigation_record_size_must_be_16[(sizeof(help_navigation_record_t) == 16) ? 1 : -1];

/* The grid help cursor over a unit's status display. A value above the text
 * ids in the pending description id (g_text_selection_id) selects one; it is latched in
 * g_screen_grid_id / g_bunit_help_screen_id instead of showing a
 * description. */
typedef enum menu_grid_help_id {
    MENU_GRID_HELP_NONE = 0,
    MENU_GRID_HELP_BANNER_TOP = 0x20000, /* the status banner is at the top of the screen */
    MENU_GRID_HELP_BANNER_BOTTOM = 0x20001,
    MENU_GRID_HELP_UNIT_BROWSE = 0x20002,
    MENU_GRID_HELP_RECRUIT_PREVIEW = 0x20003, /* WORLD: the cursor skips cell 7 */
} menu_grid_help_id_e;

extern help_navigation_record_t g_menu_help_require_navigation[];
extern help_navigation_record_t g_menu_help_terrain_navigation[];
extern help_navigation_record_t g_menu_help_unit_box_navigation[];
extern help_navigation_record_t g_menu_help_attack_preview_navigation[];

/* help screens */
/* Set while a help window is open; menu input is suppressed meanwhile. */
extern s32 g_menu_help_open;
extern u32* g_menu_help_controller_state_ptr;
extern u8 g_menu_help_data_base[];
extern menu_unit_status_record_t g_menu_help_active_banner;
extern menu_active_unit_t g_menu_help_active_unit_data;
extern menu_status_panel_slot_storage_t g_menu_help_billboard;
extern map_selected_tile_t g_menu_help_menu_cursor_tile;

/* Opens the help menu on the menu-building thread slot unless input is held off or another screen is busy. */
void menu_start_help_menu_thread(void);
/* Starts the help text thread for the pending help message: binds the help text and locks the menu input. */
void menu_text_start_help_thread(s32 thread_data);
/* Opens help for the running menu's pending request, or runs the help cursor over the navigation table of that
 * menu or the unit view. */
void menu_run_battle_help(void);
/* The help menu over the REQUIRE screen. */
void menu_run_require_help(void);

#endif
