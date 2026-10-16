#ifndef MENU_H
#define MENU_H

/* The menu library compiled into BATTLE, WORLD and the EVENT overlays: frame timing, the menu arena, menu entries with
 * their icon and number records, drawing and input routines, BATTLE's command menus and system functions, and the unit
 * status panels' records, packets, draw areas, portraits, fades and indicators. */

#include "fft/menu_script.h"
#include "psx/kernel.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

struct unit_panel_thread;

/* frame timing */
/* Raw VSync mode; animation and script steps use 1 when this is zero. */
extern s8 g_menu_event_speed;

/* Set while a menu thread forces event speed 2; the previous speed is saved. */
extern u16 g_menu_vsync_interval_forced;
extern u16 g_menu_saved_vsync_interval;

s32 menu_get_event_speed(void);
void menu_set_vsync_mode_and_event_speed(s32 speed);
s32 menu_update_menus_and_event_speed(u32* otag, s32 controller_input);

/* menu arena */
/* A first-fit allocator over 256-byte pages with sixteen allocation slots. Each program has its own arena of
 * VARIANT_MENU_BUFFER_PAGE_COUNT pages (53 in BATTLE, 112 in WORLD); menu_alloc_memory yields until a run of
 * free pages and a free slot exist. */
enum {
    MENU_BUFFER_ALLOCATION_COUNT = 16,
    MENU_BUFFER_PAGE_SIZE = 256,
};

/* One allocation: its first page, or -1 when the slot is free, and the length of the run in pages. */
typedef struct menu_allocation {
    u8* buffer;     /* 0x00 */
    s32 page_count; /* 0x04 */
} menu_allocation_t;
typedef char menu_allocation_size_must_be_8[(sizeof(menu_allocation_t) == 8) ? 1 : -1];

extern u8 g_menu_buffer_arena[][MENU_BUFFER_PAGE_SIZE];
extern menu_allocation_t g_menu_allocator_slot_records[MENU_BUFFER_ALLOCATION_COUNT];

/* Nonzero for each arena page in use. */
extern u8 g_menu_buffer_allocation_map[];

void* menu_alloc_memory(s32 size);
void menu_free_memory(void* buffer);
void menu_init_buffer_allocator(void);
void* menu_alloc_buffer(s32 bytes);

/* menu entries */
/* A scene's menus are a table of menu entries, one per window: its size and text image, its rows and their
 * actions, and the thread that runs it. The thread receives the entry as its first parameter and reads its
 * own record type through context. */

/* Record reached through menu_entry_t::context by menu_text_entry_wait_thread and WORLD's
 * screen_text_entry_thread: they call prepare, then start the text thread text_thread_id with speaker_id
 * as its third and fourth parameters. Provisional: only these members are known. */
typedef struct menu_text_binding {
    void (*prepare)(void); /* 0x00 */
    u16 speaker_id;        /* 0x04: unit whose portrait speaks (roster slot) */
    u16 text_thread_id;    /* 0x06 */
    u8 _unused_08[8];
    s16* row_text_ids; /* 0x10: text ids indexed by entry selected_index (screen_help_text_thread) */
} menu_text_binding_t;

/* Row actions 0..0xff open that menu entry; this flag suppresses the confirm sound. */
enum { MENU_ROW_ACTION_SILENT = 0x1000 };

/* Special row actions. CLEAR_SELECTION records no selection without opening another entry; NONE,
 * like any other value from 0x100 up, opens nothing, so Circle only records the row. */
enum {
    MENU_ROW_ACTION_CLEAR_SELECTION = 0x4d2,
    MENU_ROW_ACTION_NONE = 0x7d0,
};

/* Values of menu_text_layout_t.mode[]: what a column of the layout draws. */
typedef enum menu_text_column_mode {
    MENU_TEXT_COLUMN_TEXT = 0,          /* text ids, drawn by menu_display_text_entry */
    MENU_TEXT_COLUMN_NUMBER = 1,        /* two-digit numbers, drawn by menu_display_number_entry */
    MENU_TEXT_COLUMN_HIDDEN = 2,        /* the column is skipped */
    MENU_TEXT_COLUMN_NUMBER_GLYPHS = 3, /* two-digit numbers, drawn by menu_draw_large_number_glyphs */
} menu_text_column_mode_e;

/* Three-column text layout walked by menu_draw_text_columns and WORLD's screen_draw_text_columns,
 * reached through menu_entry_t::context when the entry draws a list. Column i draws mode[i] at x[i] from
 * text_ids[i] in text_colors[i], both indexed by row plus the row offset. */
typedef struct menu_text_layout {
    s16 row_count;       /* 0x00 */
    s16 hidden_rows;     /* 0x02: rows beyond the visible page (count - visible, 0 when
                            all fit); row_offset is clamped to it */
    s16 x[3];            /* 0x04 */
    s16 mode[3];         /* 0x0a: menu_text_column_mode_e */
    s16* text_ids[3];    /* 0x10 */
    s16* text_colors[3]; /* 0x1c */
} menu_text_layout_t;
typedef char menu_text_layout_size_must_be_40[sizeof(menu_text_layout_t) == 40 ? 1 : -1];

/* One menu window, shared by BATTLE, WORLD and the overlays; g_menu_thread_menu_data points at the scene's
 * current table. The icon threads read vram_origin and the inner size as the text image's upload rectangle and
 * window.x/y as the icon origin (the icon list thread clears bit 0 of window.y);
 * menu_step_wrapping_cursor_on_scroll_buttons wraps the cursor at max_row_index. */
typedef struct menu_entry {
    point16_t vram_origin; /* 0x00: text-image VRAM origin passed to the draw-mode helpers */
    s16 inner_width;       /* 0x04: text-image width; RECT.w = inner_width >> 2 (4bpp) */
    s16 inner_height;      /* 0x06: text-image height (menu_size_entry_to_text) */
    RECT window;           /* 0x08: screen position and extent passed to image and draw-area helpers */
    /* 0x10: text sprite placement (menu_build_window_sprites). width/height are set with the
     * inner and window sizes; the load origin and screen offsets are cleared
     * with them. */
    image_load_parameters_t text_image;
    s16 text_id;                 /* 0x1c: passed to text_find_entry */
    s16 max_row_index;           /* 0x1e: row count - 1 (command_build_act_menu) */
    s16 thread_count;            /* 0x20: threads menu_handle_cancel_input stops, -1 when there is no thread
                                    group; set from the system-function table by menu_run_system_function_thread */
    u8 _padding_22[0x24 - 0x22]; /* aligns row_actions */
    s16* row_actions;            /* 0x24: each row's action, indexed by selected_index */
    void (*thread_entry)(void);  /* 0x28 */
    s16 header_id;               /* 0x2c: menu header; text-thread parameter (screen_text_entry_thread); the
                                    icon threads read it signed as the icon kind: 0x10 arrow, 0..4 single
                                    icon, 5..15 four-piece icon (menu_build_window_sprites) */
    u8 _padding_2e[2];           /* aligns context */
    /* 0x30: context interpreted by the menu thread. Text threads use menu_text_binding_t
     * here, the list threads a menu_text_layout_t, the number threads a
     * screen_number_range_t and the icon threads the s16 redraw flag they
     * poll (set to 1 to rebuild the text image). */
    void* context;
    void* value;           /* 0x34: thread-specific value data: s16 result cell or main-window s32 substitution pair */
    s16 selected_index;    /* 0x38: selected row; indexes the binding's 16-bit table
                              (screen_help_text_thread) */
    s16 select_text_table; /* 0x3a: "Index for select text": row of g_screen_entry_text_id_tables
                              (0x11 = the binding's row_text_ids); 2 / -1 defer to the next running
                              thread's entry. The AT-list and option-list builders park their
                              scroll row offset here instead. */
} menu_entry_t;
typedef char menu_entry_size_must_be_60[(sizeof(menu_entry_t) == 60) ? 1 : -1];

/* Numeric-entry and action-selection threads use value as a 16-bit result cell. */
static inline s16* menu_entry_result(menu_entry_t* entry) {
    return entry->value;
}

/* icons and numbers */
/* Texture rectangle, CLUT and texture page of an icon, as the menu row callbacks return it. */
typedef struct menu_icon_source {
    s16 u;     /* 0x00 */
    s16 v;     /* 0x02 */
    s16 w;     /* 0x04 */
    s16 h;     /* 0x06 */
    u16 clut;  /* 0x08 */
    u16 tpage; /* 0x0a */
} menu_icon_source_t;
typedef char menu_icon_source_size_must_be_12[sizeof(menu_icon_source_t) == 12 ? 1 : -1];

/* One positioned number: a text position, a pointer to the live value and the decimal format.
 * menu_draw_numeric_display_entries draws tables of them (kind 0 and 1 select the plain and colored number
 * renderers), as do the status panels' menu_text_render_decimal_entry_list and
 * menu_text_render_signed_decimal_entries, which leave kind unset. */
typedef struct menu_number_entry {
    u16 x;      /* 0x00 */
    u16 y;      /* 0x02 */
    s16* value; /* 0x04 */
    s16 format; /* 0x08; low byte digit count, high byte sign/marker flags
                   (0x400/0x800/0x1000; 0xc00/0xe00 draw dashes) */
    s16 kind;   /* 0x0a */
} menu_number_entry_t;
typedef char menu_number_entry_size_must_be_12[(sizeof(menu_number_entry_t) == 12) ? 1 : -1];

/* scene state */
extern s32 g_menu_formation_display_mode;
extern s32 g_menu_post_battle_unit_changes_active;
extern u8 g_menu_common_graphics_loaded;
extern char g_menu_wait_for_file_allocation_message[];

/* The field y (0 or 0xf0) handed to menu_input_update_event_state each frame; the panels draw into the other
 * frame-buffer half. */
extern u16 g_frame_arg;

/* Advanced once per frame by the menu frame update. */
extern s32 g_frame_counter;

/* 0: normal, 1: companion/editor transition, 2: AT-list selection. */
extern u16 g_menu_overlay_state;

/* Nonzero while a screen transition holds the menus (4 during the unit list fade-out). */
extern u16 g_menu_transition_state;

/* Loads the default menu palettes and rebuilds the text section pointers. */
void menu_init_palette_and_text(s32 unused_0);

/* Binds the scene's working pointers and clears its menu-session flags. */
void menu_init_subsystem_pointers(void);

/* Frees every window buffer pair, the tpage-7 VRAM grid and the menu arena, and resets the thread scheduler. */
void menu_reset_subsystems(void);

/* 2 while a menu is being built, 4 while a menu thread runs, 0 when idle; BATTLE's body first returns
 * g_menu_transition_state when it is set. */
s32 menu_get_thread_activity(void);

/* drawing routines */
menu_draw_number_command_t* menu_draw_decimal_number(menu_draw_number_command_t* command);
menu_window_command_t* menu_draw_window_frame(menu_window_command_t* command);
void menu_gfx_enqueue_textured_quad_current_ot(const RECT* rect, s32 u, s32 v, const rgb24_t* color, s32 semitrans,
    u16 texture_page, u16 clut, s32 unused_priority);
void menu_gfx_enqueue_textured_quad_rect(
    RECT* rect, menu_icon_source_t* tex, rgb24_t* color, s32 semitrans, s32 ot_index);
void menu_update_and_draw_bouncing_cursor(point16_t* pos, u16* state, s32 mode);
void menu_draw_pressable_button(s16 index, s32 x, s16 y, s16 pressed, s16 disabled, u16 sound_effect_id);

/* input */
menu_vertical_scroll_direction_e menu_input_read_page_scroll_direction(void);
void menu_input_update_with_message_state(void);

/* menu threads */
void menu_wait_selection_threads(void);

/* Starts the help text thread for the pending help message: binds the help text and locks the menu input. */
void menu_text_start_help_thread(s32 thread_data);

/* unreferenced stubs */
/* Empty functions that nothing on the disc calls or points at; the names are neutral placeholders. */
void menu_noop_1(void);
void menu_noop_2(void);
void menu_noop_3(void);
void menu_noop_4(void);
void menu_noop_5(void);
void menu_noop_6(void);
void menu_noop_7(void);

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
    MENU_COMMAND_ID_DEBUGCHR = 44,            /* unit_editor_open */
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
    MENU_COMMAND_ID_CRYSTAL_PICKUP = 105,         /* crystal or treasure options */
} command_menu_e;

extern u8 g_menu_idle_action_window_x[];
extern u8 g_menu_skillset_types[]; /* per-row skillset kind; 13 keeps the row active */

void menu_action_dispatch(void);
command_menu_e menu_action_get_menu_id(s32 battle_id);
void menu_action_open_active_unit(void);
void menu_action_open_non_active_unit(void);

/* command resolution */
/* One row of the 31-entry menu table at g_command_command_maps; its first 12 rows are the menus whose selection is
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

extern s32 g_menu_active_menu; /* active menu */
extern s16 g_menu_result;      /* menu result: -4 pending, -1 canceled */
extern s16 g_menu_selected_option;
extern s16 g_menu_set_auto_battle_row_actions;

/* skillsets and abilities */
/* The ability list the skillset menus build in the event work block (event_work_t::ability_list): 82 rows of
 * parallel columns. ids, values and extras become the three text-id columns of g_ability_list_ability_layout.
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

/* Provisional: one 5-byte row of a menu text entry as rewritten by command_build_act_menu: the style byte at +1 is 4
 * for a grayed row, the byte at +3 is the row separator (0xfa) or the list terminator (0xff). */
typedef struct menu_text_row {
    u8 _unused_00;
    u8 style; /* 0x01 */
    u8 _unused_02;
    u8 terminator; /* 0x03: 0xfa continues, 0xff ends */
    u8 _unused_04;
} menu_text_row_t;

/* The mode argument of command_build_ability_list: which list an Arithmeticks
 * (Math Skill) menu shows. Any other menu type builds its ability list. */
typedef enum menu_arithmeticks_list {
    MENU_ARITHMETICKS_LIST_ABILITIES = 0,  /* skillset_math_collect_calculator_abilities */
    MENU_ARITHMETICKS_LIST_MULTIPLES = 1,  /* skillset_math_load_multiples */
    MENU_ARITHMETICKS_LIST_ATTRIBUTES = 2, /* skillset_math_load_attributes */
} menu_arithmeticks_list_e;

extern s32 g_menu_scrolling_list_depth;

/* The CLUT the outermost scrolling ability list saves, derives four more from and blinks while it
 * runs. */
extern RECT g_menu_ability_list_clut_rect;

/* Shared with the per-program event interpreter and dialog thread. */
u8* menu_redraw_text_page_on_scroll_in_rect(menu_entry_t* entry, s32* row_offset, s32* redraw, s32 unused, RECT* area);

/* system functions */
/* One row of g_menu_system_function_table (menu_run_system_function_thread,
 * menu_dispatch_system_function). Provisional. */
typedef struct menu_system_function {
    s16 menu_id;                /* 0x00; copied to g_command_current_id */
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
    MENU_SYSTEM_COMMAND_POST_ACTION_RESULT = 17, /* option: action_display_image_id_e */
} menu_system_command_e;

/* MENU_SYSTEM_COMMAND_TARGETING_PROMPT options. */
typedef enum menu_targeting_prompt_option {
    MENU_TARGETING_PROMPT_DEFAULT = 0,
    MENU_TARGETING_PROMPT_NO_TARGET = 1,     /* the ability preview marked no tile */
    MENU_TARGETING_PROMPT_NO_MOVE_RANGE = 2, /* the moving unit has no reachable tile */
} menu_targeting_prompt_option_e;

/* MENU_SYSTEM_COMMAND_MOVE_FEEDBACK options. Any option not named here rejects the move with the
 * invalid sound and system function option + 13. */
typedef enum menu_move_feedback_option {
    MENU_MOVE_FEEDBACK_CLOSE = 0,
    MENU_MOVE_FEEDBACK_MOUNT = 1,        /* confirm mounting the unit on the destination */
    MENU_MOVE_FEEDBACK_ILLEGAL_MOVE = 2, /* reject the chosen destination */
    MENU_MOVE_FEEDBACK_CRYSTAL = 4,      /* confirm a step onto a crystal */
} menu_move_feedback_option_e;

/* MENU_SYSTEM_COMMAND_SCENARIO_CHECK options. */
typedef enum menu_scenario_check_option {
    MENU_SCENARIO_CHECK_START = 0, /* start the event the check finds */
    MENU_SCENARIO_CHECK_SKIP = 1,  /* skip the checks and close */
    MENU_SCENARIO_CHECK_TEST = 2,  /* only record the event in g_script_next_event_id */
} menu_scenario_check_option_e;

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

extern s32 g_menu_system_function_thread_busy;
extern s32 g_menu_system_function_command;
extern s32 g_menu_system_function_option;
extern u16 g_menu_system_function_row_actions[];

/* Array view: as a scalar, GCC hoists the menu-index reload above its store
 * in menu_run_system_function_thread. */
extern menu_system_function_t g_menu_system_function_table[];

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

/* menu scrolling */
extern u8 g_menu_scroll_unread_setting; /* set to 1 with the input repeat timings; never read */
extern u8 g_menu_scroll_accel_delay;
extern u8 g_menu_scroll_slow_step;
extern u8 g_menu_scroll_fast_step;

/* unit status records */
/* The five equipment slots in the order the equipment screens list them (not
 * the order of a unit record's equipment array). */
typedef enum menu_equipment_slot {
    MENU_EQUIPMENT_SLOT_RIGHT_HAND = 0,
    MENU_EQUIPMENT_SLOT_LEFT_HAND = 1,
    MENU_EQUIPMENT_SLOT_HEAD = 2,
    MENU_EQUIPMENT_SLOT_BODY = 3,
    MENU_EQUIPMENT_SLOT_ACCESSORY = 4,
    MENU_EQUIPMENT_SLOT_HAND_COUNT = 2,
    MENU_EQUIPMENT_SLOT_COUNT = 5,
} menu_equipment_slot_e;

/* Name, job and identity values copied into WORLD and EQUIP status panels.
 * The last displayed value is the roster slot, distinct from a battle id. */
typedef struct menu_unit_status_identity {
    s16 unit_index;  /* 0x00: name text 0x4000 + value (unit_panel_banner_thread) */
    s16 job_id;      /* 0x02: job name text 0x3000 + value */
    s16 brave;       /* 0x04 */
    s16 faith;       /* 0x06 */
    s16 zodiac;      /* 0x08 */
    s16 roster_slot; /* 0x0a: party slot (formation_get_selected_unit_name_id); shown as value + 1 */
    u8 _unused_0c[2];
} menu_unit_status_identity_t;
typedef char menu_unit_status_identity_size_must_be_14[sizeof(menu_unit_status_identity_t) == 14 ? 1 : -1];

/* u16 view of the -1 stored in an empty menu_status_panel_slot_state_t.ability_ids row. */
enum {
    MENU_STATUS_PANEL_LABEL_NONE = 0xffff,
};

/* Unit stats and equipment/ability rows used by status panels and previews.
 * The arrays follow the five equipment and ability slots. */
typedef struct menu_status_panel_slot_state {
    s16 move;                     /* 0x00 */
    s16 speed;                    /* 0x02 */
    s16 jump;                     /* 0x04 */
    s16 right_weapon_power;       /* 0x06 */
    s16 left_weapon_power;        /* 0x08 */
    s16 right_weapon_evade;       /* 0x0a */
    s16 left_weapon_evade;        /* 0x0c */
    s16 generic_monster;          /* 0x0e: star icons instead of ability-type icons; selects the
                                   * ability-name text section and gates the per-slot portrait
                                   * hiding pass */
    s16 two_hands;                /* 0x10: two-hands flag */
    s16 physical_attack;          /* 0x12 */
    s16 physical_class_evade;     /* 0x14 */
    s16 physical_shield_evade;    /* 0x16 */
    s16 physical_accessory_evade; /* 0x18 */
    u8 _unused_1a[2];
    s16 magical_attack;          /* 0x1c */
    s16 magical_class_evade;     /* 0x1e */
    s16 magical_shield_evade;    /* 0x20 */
    s16 magical_accessory_evade; /* 0x22 */
    s16 equipment[5];            /* 0x24: RH, LH, helmet, armor, accessory; low byte 0xff
                                  * means "no entry"; otherwise rendered through the
                                  * item-name text section and passed to
                                  * gfx_set_item_icon_sprite */
    s16 ability_ids[5];          /* 0x2e: primary/secondary skillset, reaction, support,
                                  * movement; -1 means no label; the first two rows select
                                  * the skillset-name or ability-name text section by mode,
                                  * and the remaining rows use ability names */
} menu_status_panel_slot_state_t;
typedef char menu_status_panel_slot_state_size_must_be_56[(sizeof(menu_status_panel_slot_state_t) == 56) ? 1 : -1];

enum {
    MENU_UNIT_STATUS_GAUGE_HP = 0,
    MENU_UNIT_STATUS_GAUGE_MP = 1,
    MENU_UNIT_STATUS_GAUGE_CT = 2,
};

/* HP, MP or CT gauge, drawn 32 * value / max pixels wide. Overlay panels
 * use a four-pixel stub when max is zero; the billboard builders instead
 * store 1 for zero HP/MP maxima. Those builders clear HP/MP bonuses but leave
 * the CT bonus field untouched. */
typedef struct menu_unit_status_gauge {
    s16 value; /* 0x00 */
    s16 bonus; /* 0x02; equipment contribution or preview delta for HP/MP */
    s16 max;   /* 0x04 */
} menu_unit_status_gauge_t;
typedef char menu_unit_status_gauge_size_must_be_6[(sizeof(menu_unit_status_gauge_t) == 6) ? 1 : -1];

/* What the unit grids draw below a unit (formation_draw_unit_readout,
 * unit_list_update_grid). L2/R2 step g_formation_stat_display_mode and
 * g_unit_list_stat_mode through the first five, wrapping between HP and
 * BRAVE_FAITH; WORLD picks the others per unit. */
typedef enum menu_unit_readout {
    MENU_UNIT_READOUT_HP = 0,
    MENU_UNIT_READOUT_MP = 1,
    MENU_UNIT_READOUT_CT = 2,
    MENU_UNIT_READOUT_LEVEL_EXP = 3,
    MENU_UNIT_READOUT_BRAVE_FAITH = 4,
    MENU_UNIT_READOUT_EQUIP_LABEL = 8, /* "equipping", or "can't equip" below a value of 100 */
    MENU_UNIT_READOUT_LEAVING = 9,     /* away on a proposition */
    MENU_UNIT_READOUT_EGG = 10,        /* nothing is drawn */
    MENU_UNIT_READOUT_MISSING = 11,
} menu_unit_readout_e;

/* The battle unit stats of g_unit_panel_selected_unit. */
struct unit_stats* menu_get_selected_unit_stats(void);

/* panel threads */
/* Flags of a unit status panel (menu_status_panel_display_thread_t and
 * the WORLD status display's unit_panel_thread_t). */
typedef enum menu_status_panel_flags {
    MENU_STATUS_PANEL_SLIDE_DOWN = 0x20, /* run the slide-down table once */
    MENU_STATUS_PANEL_SLIDE_UP = 0x40,   /* run the slide-up table once */
    MENU_STATUS_PANEL_HIDDEN = 0x80,     /* draw nothing; the only flag kept when a slide ends */
} menu_status_panel_flags_e;

/* Parameter block of the editor display threads, reached through the
 * 0x400-byte battle thread table. It has the shape of unit_panel_thread_t,
 * but these threads load x as a 16-bit value. The shake offset is sign-extended
 * from the s16 shake tables and narrowed to the draw-offset y. Only the members the display
 * threads use are named; the record's total size is not proven. */
typedef struct menu_status_panel_display_thread {
    u16 x;           /* 0x00 */
    u16 _padding_02; /* aligns shake_y */
    s32 shake_y;     /* 0x04; narrowed to a draw-offset 16-bit value when rendered */
    s32 flags;       /* 0x08: menu_status_panel_flags_e */
    s32 work;        /* 0x0c: redraw request flags, cleared once serviced */
    s32 dimmed;      /* 0x10: nonzero forces the dimmed CLUTs and colors; bound as
                           g_unit_panel_status_display_dimmed and the WORLD display globals */
} menu_status_panel_display_thread_t;

/* panel packets */
/* Battle menu status and character-editor panel records. The EVENT overlays
 * (ATTACK, BUNIT, DEBUGCHR, EQUIP, REQUIRE) each link their own copy of the
 * panel code, and the WORLD unit-editor panels reuse the packet layouts. Each panel keeps two copies of its
 * packets and fills the one for this frame. */

/* DR_OFFSET followed by signed GPU offsets.
 * Signed storage retains the retail negative-immediate loads. */
typedef struct menu_status_panel_draw_offset {
    DR_OFFSET packet;   /* 0x00 */
    point16_t position; /* 0x0c */
} menu_status_panel_draw_offset_t;
typedef char menu_status_panel_draw_offset_size_must_be_16[(sizeof(menu_status_panel_draw_offset_t) == 16) ? 1 : -1];

/* Double-buffered panel packets: a center tile and nine-scanline top and
 * bottom gradients, used by status indicators and menu panel fades. */
typedef struct menu_gradient_panel_prims {
    DR_MODE draw_mode; /* 0x00 */
    TILE center;       /* 0x0c */
    TILE top[9];       /* 0x1c */
    TILE bottom[9];    /* 0xac */
} menu_gradient_panel_prims_t;
typedef char menu_gradient_panel_prims_size_must_be_316[(sizeof(menu_gradient_panel_prims_t) == 316) ? 1 : -1];

/* The numeric display frame, built by status_screen_init_numbers_frame: two DR_MODE
 * packets, twelve LINE_F2 packets and one covering SPRT whose CLUT
 * menu_init_primitive_colors_normal and _dimmed set. */
typedef struct menu_palette_primitives {
    DR_MODE draw_mode;      /* 0x00: texture window g_gfx_texture_window */
    DR_MODE draw_mode_menu; /* 0x0c: texture window g_status_screen_numbers_texture_window */
    LINE_F2 lines[12];      /* 0x18 */
    SPRT sprite;            /* 0xd8 */
} menu_palette_primitives_t;
typedef char menu_palette_primitives_size_must_be_236[(sizeof(menu_palette_primitives_t) == 236) ? 1 : -1];

/* Placement of one of the twelve frame lines (menu_palette_primitives_t.lines): signed endpoint offsets from the
 * box origin and, per coordinate, a flag selecting whether the box width or height is added. The table,
 * g_status_screen_numbers_frame_offsets, is walked by byte offset (MENU_LINE_LAYOUT_*), so nothing uses
 * this type. Provisional. */
typedef struct menu_line_layout {
    s8 x0, y0, x1, y1;                 /* 0x00 */
    s8 add_w0, add_h0, add_w1, add_h1; /* 0x04 */
} menu_line_layout_t;

/* The builder walks the table with a byte offset stepping by 8 (the
 * target's induction variable), so it addresses these fields by offset. */
enum menu_line_layout_offset {
    MENU_LINE_LAYOUT_X0 = 0,
    MENU_LINE_LAYOUT_Y0 = 1,
    MENU_LINE_LAYOUT_X1 = 2,
    MENU_LINE_LAYOUT_Y1 = 3,
    MENU_LINE_LAYOUT_ADD_W0 = 4,
    MENU_LINE_LAYOUT_ADD_H0 = 5,
    MENU_LINE_LAYOUT_ADD_W1 = 6,
    MENU_LINE_LAYOUT_ADD_H1 = 7,
    MENU_LINE_LAYOUT_SIZE = 8
};

/* Color and endpoint offsets of the Status screen panel's eight frame lines, relative to the panel box, seven bytes
 * per line (g_status_screen_line_colors). The loop walks the table with a byte offset stepping by 7 (the
 * target's induction variable and exit test), so the fields are addressed by
 * offset, as for menu_line_layout_t. */
enum menu_status_screen_line_layout_offset {
    MENU_STATUS_SCREEN_LINE_R = 0,
    MENU_STATUS_SCREEN_LINE_G = 1,
    MENU_STATUS_SCREEN_LINE_B = 2,
    MENU_STATUS_SCREEN_LINE_X0 = 3,
    MENU_STATUS_SCREEN_LINE_Y0 = 4,
    MENU_STATUS_SCREEN_LINE_X1 = 5,
    MENU_STATUS_SCREEN_LINE_Y1 = 6,
    MENU_STATUS_SCREEN_LINE_SIZE = 7
};

/* One 0x100-byte slot holding a panel's palette primitive set. */
typedef struct menu_palette_slot {
    menu_palette_primitives_t primitives; /* 0x00 */
    u8 _padding_ec[0x14];                 /* preserves the 0x100-byte allocation stride */
} menu_palette_slot_t;
typedef char menu_palette_slot_size_must_be_256[(sizeof(menu_palette_slot_t) == 256) ? 1 : -1];

/* Status-panel packet block (0x1d8 bytes), doubled the same way as the editor
 * block. sprites[6] doubles as the scroll cursor: the display threads write
 * its texture window from the scroll row/column. */
typedef struct menu_status_panel_packet {
    menu_palette_primitives_t numeric_frame;       /* 0x000 */
    SPRT sprites[7];                               /* 0x0ec */
    DR_MODE draw_mode_a;                           /* 0x178 */
    DR_MODE draw_mode_b;                           /* 0x184 */
    menu_status_panel_draw_offset_t draw_offset_a; /* 0x190 */
    menu_status_panel_draw_offset_t draw_offset_b; /* 0x1a0 */
    POLY_FT4 portrait;                             /* 0x1b0 */
} menu_status_panel_packet_t;
typedef char menu_status_panel_packet_size_must_be_472[(sizeof(menu_status_panel_packet_t) == 472) ? 1 : -1];

/* Character-editor packet block (0x30c bytes) built by the overlays' unit_panel_banner_thread. Two copies
 * sit back to back and `frame & 1` selects one, so the copy stride proves the size. The head is the numeric
 * display frame that status_screen_init_numbers_frame builds, the SPRT/POLY
 * strides come from the walks that configure and submit them, and the
 * draw-offset packets from the SetDrawOffset pairs. */
typedef struct menu_status_panel_editor_packet {
    menu_palette_primitives_t numeric_frame;       /* 0x000 */
    DR_MODE draw_mode_a;                           /* 0x0ec */
    DR_MODE draw_mode_b;                           /* 0x0f8 */
    SPRT value_sprites[4];                         /* 0x104 */
    SPRT label_sprites[7];                         /* 0x154; [6] at 0x1cc is the mode row */
    POLY_G4 bars[3];                               /* 0x1e0 */
    POLY_FT4 portrait[4];                          /* 0x24c */
    menu_status_panel_draw_offset_t draw_offset_a; /* 0x2ec */
    menu_status_panel_draw_offset_t draw_offset_b; /* 0x2fc */
} menu_status_panel_editor_packet_t;
typedef char
    menu_status_panel_editor_packet_size_must_be_780[(sizeof(menu_status_panel_editor_packet_t) == 780) ? 1 : -1];

/* The frame lines in the normal or dimmed menu colors and the covering sprite in the matching CLUT. */
void menu_init_primitive_colors_normal(menu_palette_primitives_t* menu);
void menu_init_primitive_colors_dimmed(menu_palette_primitives_t* menu);

/* number images */
/* Three 16x10 packed 4bpp images for the numbers below a unit's name:
 * Brave, Faith and the roster slot. */
typedef struct menu_unit_number_images {
    u8 brave[16 * 10 / 2];
    u8 faith[16 * 10 / 2];
    u8 roster_slot[16 * 10 / 2];
} menu_unit_number_images_t;
typedef char menu_unit_number_images_size_must_be_240[sizeof(menu_unit_number_images_t) == 240 ? 1 : -1];

/* Packed 4-bit images for the status panel's three numeric groups. */
typedef struct menu_unit_stat_number_images {
    u8 mobility[20 * 36 / 2]; /* Move, Jump and Speed. */
    u8 weapons[64 * 24 / 2];  /* Weapon power and evade. */
    u8 combat[100 * 24 / 2];  /* Physical/magical attack and class/shield/accessory evade. */
} menu_unit_stat_number_images_t;
typedef char menu_unit_stat_number_images_size_must_be_2328[sizeof(menu_unit_stat_number_images_t) == 2328 ? 1 : -1];

/* numeric panel */
/* The numeric panel's two adjacent rectangles: the first is the panel frame
 * and the screen origin handed to image loading, the second places the
 * portrait. */
typedef struct menu_status_panel_numeric_geometry {
    RECT texture_rect;   /* 0x00 */
    RECT draw_area_rect; /* 0x08 */
} menu_status_panel_numeric_geometry_t;

/* Numeric panel buffer (0x2c0 bytes), double buffered by unit_panel_numbers_thread, which copies it with sizeof. The
 * draw modes are set up with gfx_set_draw_mode_for_texture_page / SetDrawMode, the 18 SPRTs by the
 * gfx_init_image_loading walk, +0x18c and +0x298 are the arguments of status_screen_init_numbers_frame and
 * menu_init_scaled_draw_area_packets, and the draw offsets follow from the SetDrawOffset(packet, position) pairs. */
typedef struct menu_status_panel_numeric_buffer {
    DR_MODE frame_draw_mode;                         /* 0x000; MENU_TEXTURE_PAGE_FRAME */
    DR_MODE text_draw_mode;                          /* 0x00c; MENU_TEXTURE_PAGE_TEXT_IMAGES */
    DR_MODE image_draw_mode;                         /* 0x018; item icons or allocated images */
    SPRT sprites[18];                                /* 0x024 */
    menu_palette_primitives_t numeric_frame;         /* 0x18c */
    menu_status_panel_draw_offset_t draw_offsets[2]; /* 0x278 */
    scaled_draw_area_pair_t portrait;                /* 0x298 */
} menu_status_panel_numeric_buffer_t;
typedef char
    menu_status_panel_numeric_buffer_size_must_be_704[(sizeof(menu_status_panel_numeric_buffer_t) == 704) ? 1 : -1];

/* unit stats panels */
/* A stats panel's two draw offsets (unit_view_stats_panel_thread). */
typedef struct menu_unit_status_offsets {
    menu_status_panel_draw_offset_t a; /* 0x00 */
    menu_status_panel_draw_offset_t b; /* 0x10 */
} menu_unit_status_offsets_t;

/* The sprite page the stats panels draw their labels with (menu_sprite_page_t). */
extern RECT g_menu_sprite_page_texture_windows[5];
extern image_load_parameters_t g_menu_sprite_page_image_params[30];

/* Entries 5..7 of g_menu_sprite_page_image_params; a separate alias preserves the original
 * template base. */
extern image_load_parameters_t g_menu_sprite_page_image_param_bases[];

/* The final two entries of g_unit_view_attack_caster_stats_setup. */
extern menu_number_entry_t g_menu_projected_value_panel[2];

/* Live caster-panel image parameters followed by their defaults. */
extern image_load_parameters_t g_menu_attack_caster_quad_image_params[2][5];

/* scaled draw areas */
/* A panel that zooms open draws through a pair of draw areas: the panel's rectangle at the thread's position,
 * cropped about its center to g_menu_draw_area_scale_percent[scale_index] percent, and the frame buffer's own
 * area. */
extern const u16 g_menu_draw_area_scale_percent[12];
extern RECT g_menu_draw_area_template;

void menu_build_scaled_draw_area_packets(scaled_draw_area_pair_t* packet, const RECT* source, s32 scale_index,
    s32 lower_half, const struct unit_panel_thread* thread);

/* Clears the first draw area's width and installs the default second draw area. */
void menu_init_scaled_draw_area_packets(scaled_draw_area_pair_t* data);

/* status icons and zodiac */
extern u8 g_menu_gfx_scaled_sprite_count;
extern point16_t g_menu_gfx_scaled_sprite_scale[];
extern point16_t g_menu_gfx_scaled_sprite_texture_page;
extern sprite_sheet_frame_t g_menu_gfx_scaled_sprite_uv_rects[];

/* portraits */
enum {
    MENU_WLDFACE_SHEET_COUNT = 4,
    MENU_WLDFACE_SHEET_SIZE = 32768,
    MENU_WLDFACE_FILE_SIZE = MENU_WLDFACE_SHEET_COUNT * MENU_WLDFACE_SHEET_SIZE,
    MENU_WLDFACE_PORTRAITS_PER_SHEET = 40,
    MENU_WLDFACE_FIRST_MONSTER_PORTRAIT = 63,
    MENU_WLDFACE_MONSTER_PALETTE_ROW = 10, /* first row of the monsters' other palettes */
    MENU_WLDFACE_MONSTER_PALETTE_ROW_COUNT = 6,
};

/* Portrait code bits 8-9: either set means the code is not a unit portrait, so no portrait CLUT is built. */
enum { MENU_PORTRAIT_CLUT_SUPPRESS_MASK = 0x300 };

extern RECT g_menu_portrait_origin;
extern RECT g_menu_portrait_pixel_rect;
extern RECT g_menu_portrait_palette_rect;

/* panel fades */
/* g_menu_panel_fade_mode: what menu_update_panel_fade does to the panel's
 * dimming tiles. */
typedef enum menu_panel_fade_mode {
    MENU_PANEL_FADE_NONE = 0, /* not drawn */
    MENU_PANEL_FADE_IN = 1,   /* intensity rises by 8 a frame up to 80 and holds */
    MENU_PANEL_FADE_OUT = 2,  /* intensity falls by 8 a frame; at zero the mode returns to NONE */
} menu_panel_fade_mode_e;

extern s32 g_menu_panel_fade_mode; /* menu_panel_fade_mode_e */
extern s32 g_menu_panel_fade_intensity;
extern u8 g_menu_panel_fade_modes[];
extern menu_gradient_panel_prims_t g_menu_panel_fade_frames[];

void menu_update_panel_fade(void);

/* A black semi-transparent 256x50 center tile with nine one-line tiles above and below for the graded edges. */
void menu_init_panel_fade_tiles(void);

/* thread status indicators */
/* g_menu_indicator_state[] (and EQUIP's g_equipment_indicator_state[]): state of each of the two translucent
 * panels menu_draw_thread_status_indicators draws behind the unit status banners. */
typedef enum menu_indicator_state {
    MENU_INDICATOR_STATE_HIDDEN = 0,     /* not drawn */
    MENU_INDICATOR_STATE_FADING_IN = 1,  /* brightness rises by 0xc a frame up to 0x78 and holds */
    MENU_INDICATOR_STATE_FADING_OUT = 2, /* brightness falls by 0xc a frame; at zero the state returns to HIDDEN */
} menu_indicator_state_e;

extern s32 g_menu_indicator_state[2];
extern s32 g_menu_indicator_brightness[2];
extern s32 g_menu_indicator_fade_work[2];
extern s32 g_menu_indicator_packet_index[2];
extern menu_gradient_panel_prims_t g_menu_thread_indicator_packets[2][2];

void menu_draw_thread_status_indicators(void);

/* selected-tile display */
/* The panel describing the tile under the cursor. */

/* 12-byte textured-quad piece (g_unit_view_quad_pieces) that unit_view_build_height_pieces turns into a POLY_GT4:
 * texture origin, size and screen position relative to g_unit_view_quad_origin. Provisional. */
typedef struct menu_quad_piece {
    u8 u; /* 0x00 */
    u8 _unused_01;
    s8 v;           /* 0x02: texture v + 0x78 */
    u8 _padding_03; /* aligns w */
    s16 w;          /* 0x04 */
    s16 h;          /* 0x06 */
    s16 x;          /* 0x08 */
    s16 y;          /* 0x0a */
} menu_quad_piece_t;

/* The four vertex colors unit_view_build_height_pieces applies (g_unit_view_quad_colors); the leading 32-bit value is
 * untyped. Provisional. */
typedef struct menu_quad_colors {
    u32 _unused_00;
    CVECTOR vertex[4]; /* 0x04; one color per POLY_GT4 corner */
} menu_quad_colors_t;

/* One of the two 0x144-byte packet pages of the display (g_unit_view_height_display_pages): five Gouraud textured
 * quads, two sprites and the two draw-mode packets that select their texture windows. Provisional. */
typedef struct menu_quad_page {
    POLY_GT4 quads[5]; /* 0x000 */
    SPRT sprites[2];   /* 0x104 */
    DR_MODE modes[2];  /* 0x12c */
} menu_quad_page_t;
typedef char menu_quad_page_size_must_be_324[sizeof(menu_quad_page_t) == 324 ? 1 : -1];

#endif
