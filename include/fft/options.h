#ifndef FFT_OPTIONS_H
#define FFT_OPTIONS_H

#include "psx/types.h"

/*
 * Live game-options word at 0x800473ac. The OPTION overlay unpacks these
 * fields in menu order. Toggle settings encode On as 0 and Off as 1; the
 * paired high bit participates in the menu's Customize/Initialize state.
 */
enum {
    GAME_OPTIONS_THREE_BIT_VALUE_MASK = 0x7,
    GAME_OPTIONS_TWO_BIT_VALUE_MASK = 0x3,
    GAME_OPTIONS_CURSOR_MOVEMENT_SHIFT = 0,
    GAME_OPTIONS_CURSOR_MOVEMENT_MASK = 0x00000007,
    GAME_OPTIONS_CURSOR_REPEAT_SPEED_SHIFT = 3,
    GAME_OPTIONS_CURSOR_REPEAT_SPEED_MASK = 0x00000038,
    GAME_OPTIONS_MULTI_HEIGHT_CURSOR_SPEED_SHIFT = 6,
    GAME_OPTIONS_MULTI_HEIGHT_CURSOR_SPEED_MASK = 0x000001c0,
    GAME_OPTIONS_FINGER_CURSOR_REPEAT_SPEED_SHIFT = 9,
    GAME_OPTIONS_FINGER_CURSOR_REPEAT_SPEED_MASK = 0x00000e00,
    GAME_OPTIONS_MESSAGE_DISPLAY_SPEED_SHIFT = 12,
    GAME_OPTIONS_MESSAGE_DISPLAY_SPEED_MASK = 0x00007000,
    GAME_OPTIONS_NAVIGATION_MESSAGES_SHIFT = 15,
    GAME_OPTIONS_NAVIGATION_MESSAGES_MASK = 0x00018000,
    GAME_OPTIONS_ABILITY_NAMES_SHIFT = 17,
    GAME_OPTIONS_ABILITY_NAMES_MASK = 0x00060000,
    GAME_OPTIONS_EFFECT_MESSAGES_SHIFT = 19,
    GAME_OPTIONS_EFFECT_MESSAGES_MASK = 0x00180000,
    GAME_OPTIONS_SOUND_MODE_SHIFT = 21,
    GAME_OPTIONS_SOUND_MODE_MASK = 0x00600000,
    GAME_OPTIONS_SHOW_UNEQUIPPABLE_ITEMS_SHIFT = 23,
    GAME_OPTIONS_SHOW_UNEQUIPPABLE_ITEMS_MASK = 0x01800000,
    GAME_OPTIONS_DISPLAY_GAINED_EXP_JP_SHIFT = 25,
    GAME_OPTIONS_DISPLAY_GAINED_EXP_JP_MASK = 0x06000000,
    GAME_OPTIONS_TARGET_FLASHING_SHIFT = 27,
    GAME_OPTIONS_TARGET_FLASHING_MASK = 0x18000000,
    GAME_OPTIONS_MAX_EQUIP_AT_JOB_CHANGE_SHIFT = 29,
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

typedef enum game_sound_mode {
    GAME_SOUND_MODE_MONO = 0,
    GAME_SOUND_MODE_STEREO = 1,
    GAME_SOUND_MODE_WIDE = 2,
} game_sound_mode_e;

typedef struct game_options_fields {
    u32 cursor_movement : 3;
    u32 cursor_repeat_speed : 3;
    u32 multi_height_cursor_speed : 3;
    u32 finger_cursor_repeat_speed : 3;
    u32 message_display_speed : 3;
    u32 navigation_messages : 2;
    u32 ability_names : 2;
    u32 effect_messages : 2;
    u32 sound_mode : 2;
    u32 show_unequippable_items : 2;
    u32 display_gained_exp_jp : 2;
    u32 target_flashing : 2;
    u32 max_equip_at_job_change : 2;
    u32 unused_31 : 1;
} game_options_fields_t;

typedef union game_options {
    u32 value;
    game_options_fields_t fields;
} game_options_t;

typedef char game_options_size_must_be_4[sizeof(game_options_t) == 4 ? 1 : -1];

#endif
