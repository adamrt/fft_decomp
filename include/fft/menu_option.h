#ifndef MENU_OPTION_H
#define MENU_OPTION_H

/* Game options: the packed options word with its settings, and the OPTION screen that edits them (also linked
 * into WORLD). */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_window.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* game options */
/* The live options word is g_main_game_options. The options screen unpacks its fields in menu order. Toggle
 * settings encode On as 0 and Off as 1; the paired high bit participates in the menu's Customize/Initialize
 * state. */
enum {
    GAME_OPTIONS_NAVIGATION_MESSAGES_MASK = 0x00018000,
    GAME_OPTIONS_MAX_EQUIP_AT_JOB_CHANGE_MASK = 0x60000000,
    GAME_OPTIONS_NAVIGATION_MESSAGES_OFF_BITS = 0x00008000,
};

typedef enum game_option_index {
    GAME_OPTION_CURSOR_MOVEMENT = 0,
    GAME_OPTION_CURSOR_REPEAT_SPEED = 1,
    GAME_OPTION_MULTI_HEIGHT_CURSOR_SPEED = 2,
    GAME_OPTION_FINGER_CURSOR_REPEAT_SPEED = 3,
    GAME_OPTION_MESSAGE_DISPLAY_SPEED = 4,
    GAME_OPTION_NAVIGATION_MESSAGES = 5,
    GAME_OPTION_ABILITY_NAMES = 6,
    GAME_OPTION_EFFECT_MESSAGES = 7,
    GAME_OPTION_DISPLAY_GAINED_EXP_JP = 8,
    GAME_OPTION_TARGET_FLASHING = 9,
    GAME_OPTION_SHOW_UNEQUIPPABLE_ITEMS = 10,
    GAME_OPTION_MAX_EQUIP_AT_JOB_CHANGE = 11,
    GAME_OPTION_SOUND_MODE = 12,
    GAME_OPTION_COUNT = 13,
    GAME_OPTION_UNCHANGED = 13,
} game_option_index_e;

typedef enum game_option_toggle {
    GAME_OPTION_ON = 0,
    GAME_OPTION_OFF = 1,
} game_option_toggle_e;

typedef enum game_cursor_movement {
    GAME_CURSOR_MOVEMENT_TYPE_A = 0,
    GAME_CURSOR_MOVEMENT_TYPE_B = 1,
} game_cursor_movement_e;

typedef enum game_cursor_repeat_speed {
    GAME_CURSOR_REPEAT_SPEED_FAST = 0,
    GAME_CURSOR_REPEAT_SPEED_REGULAR = 1,
    GAME_CURSOR_REPEAT_SPEED_SLOW = 2,
} game_cursor_repeat_speed_e;

typedef enum game_multi_height_cursor_speed {
    GAME_MULTI_HEIGHT_CURSOR_SPEED_FAST = 0,
    GAME_MULTI_HEIGHT_CURSOR_SPEED_REGULAR = 1,
    GAME_MULTI_HEIGHT_CURSOR_SPEED_SLOW = 2,
    GAME_MULTI_HEIGHT_CURSOR_SPEED_STOP = 3,
} game_multi_height_cursor_speed_e;

typedef enum game_finger_cursor_repeat_speed {
    GAME_FINGER_CURSOR_REPEAT_SPEED_FASTEST = 0,
    GAME_FINGER_CURSOR_REPEAT_SPEED_FASTER = 1,
    GAME_FINGER_CURSOR_REPEAT_SPEED_FAST = 2,
    GAME_FINGER_CURSOR_REPEAT_SPEED_REGULAR = 3,
    GAME_FINGER_CURSOR_REPEAT_SPEED_SLOW = 4,
    GAME_FINGER_CURSOR_REPEAT_SPEED_SLOWER = 5,
    GAME_FINGER_CURSOR_REPEAT_SPEED_SLOWEST = 6,
} game_finger_cursor_repeat_speed_e;

typedef enum game_message_display_speed {
    GAME_MESSAGE_DISPLAY_SPEED_FAST = 0,
    GAME_MESSAGE_DISPLAY_SPEED_REGULAR = 1,
    GAME_MESSAGE_DISPLAY_SPEED_SLOW = 2,
} game_message_display_speed_e;

typedef struct game_options_fields {
    u32 cursor_movement : 3;
    u32 cursor_repeat_speed : 3;
    u32 multi_height_cursor_speed : 3;
    u32 finger_cursor_repeat_speed : 3;
    u32 message_display_speed : 3;
    u32 navigation_messages : 2;
    u32 ability_names : 2;
    u32 effect_messages : 2;
    u32 sound_mode : 2; /* suzuki_sound_type_e */
    u32 show_unequippable_items : 2;
    u32 display_gained_exp_jp : 2;
    u32 target_flashing : 2;
    u32 max_equip_at_job_change : 2;
    u32 _unused_03_bit7 : 1;
} game_options_fields_t;

/* The reset loads and stores this four-byte record as a scalar; a struct copy
 * moves the defaults load ahead of the input-timing stores. */
typedef union game_options {
    u32 value;
    game_options_fields_t fields;
} game_options_t;
typedef char game_options_size_must_be_4[sizeof(game_options_t) == 4 ? 1 : -1];

/* The player's options, saved while a tutorial event forces its own and restored when it ends. */
extern game_options_t g_menu_saved_game_options;

/* options screen */
/* The argument of option_entrypoint and world_menu_run_main_mode: which of
 * the OPTION overlay's screens to run. */
typedef enum menu_option_type {
    MENU_OPTION_TYPE_GAME_OPTIONS = 0,
    MENU_OPTION_TYPE_AT_LIST = 1,
    MENU_OPTION_TYPE_CRYSTAL_PICKUP = 2, /* crystal and treasure panel */
} menu_option_type_e;

/* One of the two 0x190-byte packet pages of the options screen (g_menu_option_pages), alternated by frame.
 * Provisional. */
typedef struct options_menu_page {
    SPRT title;                         /* 0x000: window title image */
    SPRT value;                         /* 0x014: selected value image */
    scaled_draw_area_pair_t title_area; /* 0x028 */
    scaled_draw_area_pair_t value_area; /* 0x050 */
    DR_MODE text_mode;                  /* 0x078 */
    DR_MODE window_mode;                /* 0x084 */
    DR_MODE icon_mode;                  /* 0x090 */
    menu_icon_record_t icons;           /* 0x09c */
    SPRT frame;                         /* 0x118: cursor frame */
    SPRT arrows[2];                     /* 0x12c: scroll arrows */
    SPRT arrow_marks[2];                /* 0x154 */
    SPRT thumb;                         /* 0x17c: scroll-bar thumb */
} options_menu_page_t;
typedef char options_menu_page_size_must_be_400[(sizeof(options_menu_page_t) == 400) ? 1 : -1];

extern options_menu_page_t g_menu_option_pages[2];
extern menu_text_layout_t g_menu_option_entry_table[1];
extern menu_entry_t g_menu_option_icon_thread_param;
extern menu_icon_record_t g_menu_option_picker_icon_records[2];
/* The values being edited, one per game_option_index_e. */
extern s32 g_menu_option_values[GAME_OPTION_UNCHANGED + 1];
extern s32 g_menu_option_reference_values[];
extern s32 g_menu_option_choice_counts[];
extern s32 g_menu_option_choice_widths[];
extern s32 g_menu_option_value_text_id_bases[];
extern RECT g_menu_option_title_image_rect;
extern RECT g_menu_option_entry_panel_rect;
extern RECT g_menu_option_value_image_rect;

/* The value picker thread of one game option. */
void menu_run_option_choice(void);

#endif
