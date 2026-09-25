#ifndef FFT_SCRIPT_H
#define FFT_SCRIPT_H

/* Event-script bytecode and variables shared by the BATTLE and WORLD interpreters. */

#include "fft/unit.h"
#include "psx/types.h"

/* event */
enum {
    EVENT_BLOCK_BYTES = 8192,
    EVENT_BLOCK_SECTORS = 4,
    EVENT_ARCHIVE_START_SECTOR = 0xe7b,
    EVENT_OPCODE_COUNT = 256,
    /* Number of simulation unit slots an interpreter scans by misc id. */
    EVENT_UNIT_SLOT_COUNT = BATTLE_UNIT_SLOT_COUNT,
    /* External unit-id selectors used by scenario commands. */
    EVENT_UNIT_SELECTOR_SLOT_FIRST = 0x64,
    EVENT_UNIT_SELECTOR_SLOT_COUNT = 4,
    EVENT_UNIT_SELECTOR_ACTIVE_TURN = 0x69,
    /* Five ordinary deployed-unit IDs; the end value is exclusive. */
    EVENT_UNIT_ID_DEPLOYED_FIRST = 0x78,
    EVENT_UNIT_ID_DEPLOYED_END = 0x7d,
    /* battle_get_misc_id / world_get_misc_id result for an absent unit. */
    EVENT_MISC_ID_NONE = 2000,
    /* g_*_event_unit_slots[] entry with no ghost unit assigned. */
    EVENT_UNIT_SLOT_EMPTY = 1000,
};

#define EVENT_TEXT_OFFSET_PROCESSED 0xf2f2f2f2U

/* Retail scenario bytecode, reviewed against BATTLE 0x80143bd8..0x80145f78 and
 * the shared BATTLE/WORLD operand-length tables. Comments count encoded
 * operand BYTES, excluding the opcode; they do not specify parameter widths.
 * UNKNOWN values are explicit dispatch cases, not guesses for otherwise
 * unrecognized byte values. Established command labels describe BATTLE; WORLD
 * can handle them differently. BETA does not imply unused. JUMP animates a
 * unit, whereas JUMP_FORWARD/JUMP_BACK branch within the script. CALL_FUNCTION
 * selects a built-in operation, not a native function pointer. EFFECT_END
 * starts the staged effect; it does not cancel it. */
typedef enum event_opcode {
    EVENT_OPCODE_DISPLAY_MESSAGE = 0x10,        /* 14 */
    EVENT_OPCODE_UNIT_ANIM = 0x11,              /* 5 */
    EVENT_OPCODE_UNKNOWN_12 = 0x12,             /* 2 */
    EVENT_OPCODE_CHANGE_MAP_BETA = 0x13,        /* 2 */
    EVENT_OPCODE_PAUSE = 0x16,                  /* 0 */
    EVENT_OPCODE_UNKNOWN_17 = 0x17,             /* 5; recognized no-op */
    EVENT_OPCODE_EFFECT = 0x18,                 /* 6 */
    EVENT_OPCODE_CAMERA = 0x19,                 /* 16 */
    EVENT_OPCODE_MAP_DARKNESS = 0x1a,           /* 5 */
    EVENT_OPCODE_MAP_LIGHT = 0x1b,              /* 14 */
    EVENT_OPCODE_EVENT_SPEED = 0x1c,            /* 1 */
    EVENT_OPCODE_CAMERA_FUSION_START = 0x1d,    /* 0 */
    EVENT_OPCODE_CAMERA_FUSION_END = 0x1e,      /* 0 */
    EVENT_OPCODE_FOCUS = 0x1f,                  /* 5 */
    EVENT_OPCODE_SOUND_EFFECT = 0x21,           /* 2 */
    EVENT_OPCODE_SWITCH_TRACK = 0x22,           /* 3 */
    EVENT_OPCODE_UNKNOWN_23 = 0x23,             /* 3; recognized no-op */
    EVENT_OPCODE_UNKNOWN_24 = 0x24,             /* 0; recognized no-op */
    EVENT_OPCODE_UNKNOWN_25 = 0x25,             /* 4 */
    EVENT_OPCODE_UNKNOWN_26 = 0x26,             /* 2 */
    EVENT_OPCODE_RELOAD_MAP_STATE = 0x27,       /* 0 */
    EVENT_OPCODE_WALK_TO = 0x28,                /* 8 */
    EVENT_OPCODE_WAIT_WALK = 0x29,              /* 2 */
    EVENT_OPCODE_BLOCK_START = 0x2a,            /* 0 */
    EVENT_OPCODE_BLOCK_END = 0x2b,              /* 0 */
    EVENT_OPCODE_FACE_UNIT_2 = 0x2c,            /* 7 */
    EVENT_OPCODE_ROTATE_UNIT = 0x2d,            /* 6 */
    EVENT_OPCODE_BACKGROUND = 0x2e,             /* 8 */
    EVENT_OPCODE_COLOR_BG_BETA = 0x31,          /* 5 */
    EVENT_OPCODE_COLOR_UNIT = 0x32,             /* 7 */
    EVENT_OPCODE_COLOR_FIELD = 0x33,            /* 5 */
    EVENT_OPCODE_UNKNOWN_37 = 0x37,             /* 2 */
    EVENT_OPCODE_FOCUS_SPEED = 0x38,            /* 2 */
    EVENT_OPCODE_WAIT_ALL_UNIT_MOVEMENT = 0x39, /* 0 */
    EVENT_OPCODE_WAIT_FILE_LOAD = 0x3a,         /* 0 */
    EVENT_OPCODE_SPRITE_MOVE = 0x3b,            /* 12 */
    EVENT_OPCODE_WEATHER = 0x3c,                /* 2 */
    EVENT_OPCODE_REMOVE_UNIT = 0x3d,            /* 2 */
    EVENT_OPCODE_COLOR_SCREEN = 0x3e,           /* 9 */
    EVENT_OPCODE_SET_TILE_CAMERA_BLOCK = 0x3f,  /* 4 */
    EVENT_OPCODE_UNKNOWN_40 = 0x40,             /* 5 */
    EVENT_OPCODE_EARTHQUAKE_START = 0x41,       /* 4 */
    EVENT_OPCODE_EARTHQUAKE_END = 0x42,         /* 0 */
    EVENT_OPCODE_CALL_FUNCTION = 0x43,          /* 1 */
    EVENT_OPCODE_DRAW = 0x44,                   /* 2 */
    EVENT_OPCODE_ADD_UNIT = 0x45,               /* 3 */
    EVENT_OPCODE_ERASE = 0x46,                  /* 2 */
    EVENT_OPCODE_ADD_GHOST_UNIT = 0x47,         /* 8 */
    EVENT_OPCODE_WAIT_ADD_UNIT = 0x48,          /* 0 */
    EVENT_OPCODE_ADD_UNIT_START = 0x49,         /* 0 */
    EVENT_OPCODE_ADD_UNIT_END = 0x4a,           /* 0 */
    EVENT_OPCODE_WAIT_ADD_UNIT_END = 0x4b,      /* 0 */
    EVENT_OPCODE_CHANGE_MAP = 0x4c,             /* 2 */
    EVENT_OPCODE_UNKNOWN_4D = 0x4d,             /* 1 */
    EVENT_OPCODE_UNIT_SHADOW = 0x4e,            /* 3 */
    EVENT_OPCODE_SET_DAYTIME = 0x4f,            /* 1 */
    EVENT_OPCODE_PORTRAIT_COL = 0x50,           /* 1 */
    EVENT_OPCODE_CHANGE_DIALOG = 0x51,          /* 5 */
    EVENT_OPCODE_FACE_UNIT = 0x53,              /* 7 */
    EVENT_OPCODE_USE_3D_OBJECT = 0x54,          /* 2 */
    EVENT_OPCODE_USE_FIELD_OBJECT = 0x55,       /* 2 */
    EVENT_OPCODE_WAIT_3D_OBJECT = 0x56,         /* 0 */
    EVENT_OPCODE_WAIT_FIELD_OBJECT = 0x57,      /* 0 */
    EVENT_OPCODE_LOAD_EVTCHR = 0x58,            /* 3 */
    EVENT_OPCODE_SAVE_EVTCHR = 0x59,            /* 1 */
    EVENT_OPCODE_SAVE_EVTCHR_CLEAR = 0x5a,      /* 1 */
    EVENT_OPCODE_LOAD_EVTCHR_CLEAR = 0x5b,      /* 1 */
    /* Deferred requests; copy can be refused, release does not erase pixels. */
    EVENT_OPCODE_COPY_UNIT_GRAPHICS_TO_RESERVED_VRAM_SLOT = 0x5c, /* 3 */
    EVENT_OPCODE_RELEASE_RESERVED_VRAM_SLOT = 0x5d,               /* 1 */
    EVENT_OPCODE_END_TRACK = 0x5e,                                /* 1 */
    EVENT_OPCODE_WARP_UNIT = 0x5f,                                /* 6 */
    EVENT_OPCODE_FADE_SOUND = 0x60,                               /* 2 */
    EVENT_OPCODE_UNKNOWN_61 = 0x61,                               /* 3; recognized no-op */
    EVENT_OPCODE_SET_BATTLE_UNIT_POSITION = 0x62,                 /* 6; byte slot, unused byte, x/y/elevation/facing */
    EVENT_OPCODE_CAMERA_SPEED_CURVE = 0x63,                       /* 1 */
    EVENT_OPCODE_WAIT_ROTATE_UNIT = 0x64,                         /* 2 */
    EVENT_OPCODE_WAIT_ROTATE_ALL = 0x65,                          /* 0 */
    EVENT_OPCODE_UNKNOWN_66 = 0x66,                               /* 0 */
    EVENT_OPCODE_MIRROR_SPRITE = 0x68,                            /* 3 */
    EVENT_OPCODE_FACE_TILE = 0x69,                                /* 8 */
    EVENT_OPCODE_EDIT_BG_SOUND = 0x6a,                            /* 5 */
    EVENT_OPCODE_BG_SOUND = 0x6b,                                 /* 5 */
    EVENT_OPCODE_UNKNOWN_6C = 0x6c,                               /* 2 */
    EVENT_OPCODE_UNKNOWN_6D = 0x6d,                               /* 2 */
    EVENT_OPCODE_SPRITE_MOVE_BETA = 0x6e,                         /* 12 */
    EVENT_OPCODE_WAIT_SPRITE_MOVE = 0x6f,                         /* 2 */
    EVENT_OPCODE_JUMP = 0x70,                                     /* 4 */
    EVENT_OPCODE_SET_FIRST_MISC_UNIT = 0x71,                      /* 2 */
    EVENT_OPCODE_UNKNOWN_72 = 0x72,                               /* 0 */
    EVENT_OPCODE_UNKNOWN_73 = 0x73,                               /* 14 */
    EVENT_OPCODE_UNKNOWN_74 = 0x74,                               /* 6 */
    EVENT_OPCODE_UNKNOWN_75 = 0x75,                               /* 6; recognized no-op */
    EVENT_OPCODE_DARK_SCREEN = 0x76,                              /* 6 */
    EVENT_OPCODE_REMOVE_DARK_SCREEN = 0x77,                       /* 0 */
    EVENT_OPCODE_DISPLAY_CONDITIONS = 0x78,                       /* 2 */
    EVENT_OPCODE_WALK_TO_ANIM = 0x79,                             /* 4 */
    EVENT_OPCODE_DISMISS_UNIT = 0x7a,                             /* 2 */
    EVENT_OPCODE_TUTORIAL_BUTTON_WAIT = 0x7b,                     /* 2 */
    EVENT_OPCODE_END_SOUND = 0x7c,                                /* 0 */
    EVENT_OPCODE_SHOW_GRAPHIC = 0x7d,                             /* 1 */
    EVENT_OPCODE_WAIT_VALUE = 0x7e,                               /* 4 */
    EVENT_OPCODE_EVTCHR_PALETTE = 0x7f,                           /* 4 */
    EVENT_OPCODE_MARCH = 0x80,                                    /* 3 */
    EVENT_OPCODE_UNKNOWN_81 = 0x81,                               /* 3 */
    EVENT_OPCODE_LOAD_ATTACK_GRAPHICS = 0x82,                     /* 0 */
    EVENT_OPCODE_CHANGE_STATS = 0x83,                             /* 5 */
    EVENT_OPCODE_PLAY_TUNE = 0x84,                                /* 1 */
    EVENT_OPCODE_UNLOCK_DATE = 0x85,                              /* 1 */
    EVENT_OPCODE_TEMP_WEAPON = 0x86,                              /* 3 */
    EVENT_OPCODE_ARROW = 0x87,                                    /* 4 */
    EVENT_OPCODE_MAP_UNFREEZE = 0x88,                             /* 0 */
    EVENT_OPCODE_MAP_FREEZE = 0x89,                               /* 0 */
    EVENT_OPCODE_EFFECT_START = 0x8a,                             /* 0 */
    EVENT_OPCODE_EFFECT_END = 0x8b,                               /* 0 */
    EVENT_OPCODE_UNIT_ANIM_ROTATE = 0x8c,                         /* 6 */
    EVENT_OPCODE_WAIT_GRAPHIC_PRINT = 0x8e,                       /* 0 */
    EVENT_OPCODE_LOCK_MENU_INPUT = 0x8f,                          /* 1 */
    EVENT_OPCODE_TUTORIAL_COORDINATE_CONFIRM = 0x90,              /* 3 */
    EVENT_OPCODE_SHOW_MAP_TITLE = 0x91,                           /* 3 */
    EVENT_OPCODE_INFLICT_STATUS = 0x92,                           /* 5 */
    EVENT_OPCODE_SET_DATE_ADVANCE = 0x93,                         /* 2 */
    EVENT_OPCODE_TELEPORT_OUT = 0x94,                             /* 2 */
    EVENT_OPCODE_UNKNOWN_95 = 0x95,                               /* 0; recognized no-op */
    EVENT_OPCODE_APPEND_MAP_STATE = 0x96,                         /* 0 */
    EVENT_OPCODE_RESET_PALETTE = 0x97,                            /* 2 */
    EVENT_OPCODE_TELEPORT_IN = 0x98,                              /* 2 */
    EVENT_OPCODE_BLUE_REMOVE_UNIT = 0x99,                         /* 2 */

    /* Compare script operands 0 and 1; write the boolean to operand 0. */
    EVENT_CONDITION_SECOND_GTE_FIRST = 0xa0, /* 0 */
    EVENT_CONDITION_FIRST_GTE_SECOND = 0xa1, /* 0 */
    EVENT_CONDITION_EQUAL = 0xa2,            /* 0 */
    EVENT_CONDITION_NOT_EQUAL = 0xa3,        /* 0 */
    EVENT_CONDITION_FIRST_LT_SECOND = 0xa4,  /* 0 */
    EVENT_CONDITION_SECOND_LT_FIRST = 0xa5,  /* 0 */

    /* Update a destination variable using an immediate or variable value.
     * ZERO_VARIABLE only encodes the destination. */
    EVENT_OPCODE_ADD_IMMEDIATE = 0xb0,      /* 4 */
    EVENT_OPCODE_ADD_VARIABLE = 0xb1,       /* 4 */
    EVENT_OPCODE_SUBTRACT_IMMEDIATE = 0xb2, /* 4 */
    EVENT_OPCODE_SUBTRACT_VARIABLE = 0xb3,  /* 4 */
    EVENT_OPCODE_MULTIPLY_IMMEDIATE = 0xb4, /* 4 */
    EVENT_OPCODE_MULTIPLY_VARIABLE = 0xb5,  /* 4 */
    EVENT_OPCODE_DIVIDE_IMMEDIATE = 0xb6,   /* 4 */
    EVENT_OPCODE_DIVIDE_VARIABLE = 0xb7,    /* 4 */
    EVENT_OPCODE_MODULO_IMMEDIATE = 0xb8,   /* 4 */
    EVENT_OPCODE_MODULO_VARIABLE = 0xb9,    /* 4 */
    EVENT_OPCODE_AND_IMMEDIATE = 0xba,      /* 4 */
    EVENT_OPCODE_AND_VARIABLE = 0xbb,       /* 4 */
    EVENT_OPCODE_OR_IMMEDIATE = 0xbc,       /* 4 */
    EVENT_OPCODE_OR_VARIABLE = 0xbd,        /* 4 */
    EVENT_OPCODE_ZERO_VARIABLE = 0xbe,      /* 2 */

    EVENT_OPCODE_UNKNOWN_C0 = 0xc0,           /* 16; recognized no-op */
    EVENT_OPCODE_JUMP_FORWARD_IF_ZERO = 0xd0, /* 1 */
    EVENT_OPCODE_JUMP_FORWARD = 0xd1,         /* 1 */
    EVENT_OPCODE_FORWARD_TARGET = 0xd2,       /* 1 */
    EVENT_OPCODE_JUMP_BACK = 0xd3,            /* 1 */
    EVENT_OPCODE_UNKNOWN_D4 = 0xd4,           /* 1; stops the current task */
    EVENT_OPCODE_BACK_TARGET = 0xd5,          /* 1 */
    EVENT_OPCODE_UNKNOWN_D8 = 0xd8,           /* 1; recognized no-op */
    EVENT_OPCODE_UNKNOWN_D9 = 0xd9,           /* 1; recognized no-op */
    EVENT_OPCODE_UNKNOWN_DA = 0xda,           /* 0; recognized no-op */
    EVENT_OPCODE_END = 0xdb,                  /* 0 */
    EVENT_OPCODE_UNKNOWN_DC = 0xdc,           /* 0 */
    EVENT_OPCODE_UNKNOWN_E0 = 0xe0,           /* 2; recognized no-op */
    EVENT_OPCODE_END_2 = 0xe3,                /* 0 */
    EVENT_OPCODE_WAIT_FOR_INSTRUCTION = 0xe5, /* 2 */
    EVENT_OPCODE_YIELD = 0xf0,                /* 0 */
    EVENT_OPCODE_WAIT = 0xf1,                 /* 2 */
    EVENT_OPCODE_NOP = 0xf2,                  /* 0 */
} event_opcode_e;

/* Built-in operation selected by EVENT_OPCODE_CALL_FUNCTION. The PSX domain
 * is 0x01..0x12 and is shared by the structurally parallel BATTLE and WORLD
 * interpreters. BLOCK_START handles only the mute/unmute selectors. */
typedef enum event_call_function {
    EVENT_CALL_FUNCTION_JOIN_UNITS_SILENTLY = 0x01,
    EVENT_CALL_FUNCTION_ADVANCE_YEAR = 0x02,
    EVENT_CALL_FUNCTION_INCREASE_BLUE_TEAM_BRAVE = 0x03,
    EVENT_CALL_FUNCTION_REMOVE_ALL_ENEMY_UNITS = 0x04,
    EVENT_CALL_FUNCTION_UNKNOWN_05 = 0x05,
    EVENT_CALL_FUNCTION_OPEN_SAVE_MENU = 0x06,
    EVENT_CALL_FUNCTION_ADVANCE_YEAR_ALTERNATE = 0x07,
    EVENT_CALL_FUNCTION_UNKNOWN_08 = 0x08,
    EVENT_CALL_FUNCTION_MUTE_TEXT_AUDIO_CUE = 0x09,
    EVENT_CALL_FUNCTION_UNMUTE_TEXT_AUDIO_CUE = 0x0a,
    EVENT_CALL_FUNCTION_ADVANCE_MAP_DESTRUCTION_AND_WAIT = 0x0b,
    EVENT_CALL_FUNCTION_PLAY_ALTIMA_MUSIC = 0x0c,
    EVENT_CALL_FUNCTION_COPY_UNIT_8_POSITION_TO_UNIT_9_AND_RELOAD_GRAPHICS = 0x0d,
    EVENT_CALL_FUNCTION_RESET_BETHLA_REGION_UNIT_GRAPHICS = 0x0e,
    EVENT_CALL_FUNCTION_WARP_UNIT_4_DISPLAY_TO_UNIT_2_POSITION_AND_FACING = 0x0f,
    EVENT_CALL_FUNCTION_WARP_UNIT_5_DISPLAY_TO_UNIT_3_POSITION_AND_FACING = 0x10,
    EVENT_CALL_FUNCTION_COPY_UNIT_2_POSITION_AND_FACING_TO_UNIT_8_AND_RELOAD_GRAPHICS = 0x11,
    EVENT_CALL_FUNCTION_CLEAR_ZALERA_ACTIVE_STATUS_GRAPHICS = 0x12,
} event_call_function_e;

/* EVENT/TEST.EVT record. The first word is relative to the block base, not
 * data[]. battle_script_load_event converts it to a text pointer, then overwrites
 * it with EVENT_TEXT_OFFSET_PROCESSED. This is not a decoded instruction
 * array or a native scheduler context. The interpreter starts at byte zero:
 * the processed header is four one-byte 0xf2 NOPs before data[]. */
typedef struct event_file_block {
    u32 text_offset_or_marker;
    u8 data[EVENT_BLOCK_BYTES - 4];
} event_file_block_t;
typedef char event_file_block_size_must_be_8192[(sizeof(event_file_block_t) == EVENT_BLOCK_BYTES) ? 1 : -1];

/* Eight-byte operand payload for event opcode 0x28 WalkTo. Multi-byte values
 * remain byte arrays because event scripts are not guaranteed to be aligned. */
typedef struct event_walk_to_parameters {
    u8 unit_id_le[2];
    u8 x;
    u8 y;
    u8 elevation;
    u8 speed_le[2];
    u8 elevation_flag;
} event_walk_to_parameters_t;
typedef char event_walk_to_parameters_size_must_be_8[sizeof(event_walk_to_parameters_t) == 8 ? 1 : -1];

/* Provisional event work block at *g_battle_ai_workspace_ptr; only the staged-status
 * snapshot at 0x5f0 is known. */
typedef struct world_event_work {
    u8 unknown_000[0x5F0];
    unit_status_staging_t status_staging; /* 0x5f0 */
} world_event_work_t;
typedef char world_event_work_size_must_be_0x9e2[(sizeof(world_event_work_t) == 0x9E2) ? 1 : -1];

/* variables */
/* Event-script variable ids shared by the BATTLE and WORLD interpreters.
 * The calendar and location ids are also serialized into the memory-card
 * slot description by CARD 0x801c01ac. */
typedef enum event_script_variable_id {
    /* Result word tested by the conditional-jump opcodes. */
    EVENT_SCRIPT_VAR_COMPARISON_RESULT = 0x00,
    EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION = 0x18,
    /* First of the camera X, Z, Y and angle words (0x1a-0x1d) handed to the
     * camera opcodes as one block. */
    EVENT_SCRIPT_VAR_CAMERA_X = 0x1a,
    EVENT_SCRIPT_VAR_CAMERA_ANGLE = 0x1d,
    /* WORLD camera setup stores its 12-bit yaw here; the corresponding reader
     * normalizes it into 0..0xfff. */
    EVENT_SCRIPT_VAR_CAMERA_YAW = 0x1e,
    EVENT_SCRIPT_VAR_CAMERA_ROTATION = 0x1f,
    /* Reads synthesize a fresh random value; the backing word does not retain
     * the value returned by the interpreter. */
    EVENT_SCRIPT_VAR_RANDOM_VALUE = 0x21,
    EVENT_SCRIPT_VAR_WEATHER_AND_TIME = 0x22,
    EVENT_SCRIPT_VAR_WEATHER = 0x23,
    EVENT_SCRIPT_VAR_TIME_OF_DAY = 0x24,
    EVENT_SCRIPT_VAR_CURRENT_EVENT = 0x27,
    EVENT_SCRIPT_VAR_DISABLED_MENU_ACTIONS = 0x28,
    EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS = 0x29,
    EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS = 0x2a,
    EVENT_SCRIPT_VAR_WAR_FUNDS = 0x2c,
    EVENT_SCRIPT_VAR_MONTH = 0x2e,
    EVENT_SCRIPT_VAR_DAY = 0x2f,
    EVENT_SCRIPT_VAR_MAP_ARRANGEMENT = 0x30,
    EVENT_SCRIPT_VAR_LOCATION = 0x31,
    EVENT_SCRIPT_VAR_CURRENT_ENTD = 0x32,
    EVENT_SCRIPT_VAR_CURRENT_MAP = 0x33,
    EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT = 0x34,
    EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID = 0x35,
    EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID = 0x36,
    /* ATTACK stores the selected squad's unit limit here; SMALL clears it
     * during overlay setup. */
    EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT = 0x39,
    /* Set while CallFunction 0x06 saves the game. */
    EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS = 0x51,
    EVENT_SCRIPT_VAR_DEPLOYED_UNIT_COUNT = 0x52,
    EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE = 0x53,
    EVENT_SCRIPT_VAR_DATE_ADVANCE = 0x54,
    EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT = 0x55,
    EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE = 0x56,
    /* Incremented for every glyph the dialog typewriter uploads. */
    EVENT_SCRIPT_VAR_PRINTED_CHARACTER_COUNT = 0x57,
    /* Added to the typewriter's VRAM x column (0x1c0); the dialog threads
     * clear it when they start. */
    EVENT_SCRIPT_VAR_TYPEWRITER_VRAM_X_OFFSET = 0x59,
    /* Written by text control TEXT_SET_PORTRAIT. */
    EVENT_SCRIPT_VAR_DIALOG_PORTRAIT = 0x5a,
    /* The Deep Dungeon location menu stores the chosen floor here. WLDCORE
     * also stages a script's pending sound effect, weather sound and music
     * track in 0x5c-0x5e and clears all three once they have played. */
    EVENT_SCRIPT_VAR_DEEP_DUNGEON_SELECTION = 0x5c,
    EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT = 0x5c,
    EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND = 0x5d,
    EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK = 0x5e,
    /* The player character's birthday, written by the OPEN birthday entry. */
    EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH = 0x5f,
    EVENT_SCRIPT_VAR_PLAYER_BIRTH_DAY = 0x60,
    EVENT_SCRIPT_VAR_INJURED = 0x61,
    EVENT_SCRIPT_VAR_CASUALTIES = 0x62,
    /* Enables the contextual Anything entry in the action skillset menu. */
    EVENT_SCRIPT_VAR_ANYTHING_ACTION_ENABLED = 0x63,
    EVENT_SCRIPT_VAR_NEXT_SCENARIO = 0x64,
    /* One less than the number of Deep Dungeon floors listed. */
    EVENT_SCRIPT_VAR_DEEP_DUNGEON_LIST_LENGTH = 0x65,
    /* Scaled by 10 and added to the TEXT_SET_VARIABLE_BASE variable by
     * TEXT_STORE_VARIABLE, which then clears it. */
    EVENT_SCRIPT_VAR_TEXT_STORE_INDEX = 0x66,
    /* Incremented each March 21 and by whole years of advanced days, capped
     * at 99. */
    EVENT_SCRIPT_VAR_UNKNOWN_67 = 0x67,
    EVENT_SCRIPT_VAR_DEEP_DUNGEON_EXIT = 0x68,
    /* WLDCORE also uses this value to select the current-location marker's
     * animation on the world map. */
    EVENT_SCRIPT_VAR_TOWN_BACKGROUND = 0x69,
    EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY = 0x6f,
    /* Special misc-unit count (capped at 4) that selects the Deep Dungeon
     * map state; a change reloads the map. */
    EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT = 0x70,
    EVENT_SCRIPT_VAR_FUR_SHOP_ENABLED = 0x90,
    EVENT_SCRIPT_VAR_PROPOSITIONS_ENABLED = 0x91,
    EVENT_SCRIPT_VAR_FACTS_ENABLED = 0x92,
    EVENT_SCRIPT_VAR_OLAN_SAW_RAMZA_FUNERAL = 0xa0,
    /* When set, the interpreters skip the DismissUnit, CallFunction,
     * UnlockDate, and SetDateAdvance side effects. */
    EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS = 0x1fc,
    /* Read at interpreter start and cleared at EventEnd. */
    EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS = 0x1fd,
    /* ATTACK uses this branch while selecting deployment squads, and REQUIRE
     * uses it while applying permanent Brave and Faith changes. */
    EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE = 0x1fe,
    EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD = 0x1ff,
} event_script_variable_id_e;

#endif
