#ifndef FFT_EVENT_H
#define FFT_EVENT_H

#include "fft/battle_runtime.h"
#include "fft/thread.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

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

typedef char event_file_block_size_must_be_8192[(sizeof(event_file_block_t) == EVENT_BLOCK_BYTES) ? 1 : -1];

/* Separate triggers, command arguments, and completion status. Argument
 * domains remain provisional: map commands 0x80/0x83, polls 0x81/0x84.
 * The u16 requests and arguments are read with lhu by the consumer
 * battle_script_process_pending_requests; every other access is a store. */
extern u16 g_battle_3d_object_use_request; /* 0x80165fe2 */
extern u16 g_battle_3d_object_wait_status; /* 0x8016606e */

/* 0x80166054: camera consumer unpacks two 2-bit fields and a high nibble. */
extern s32 g_battle_camera_speed_curve;
extern u16 g_battle_current_music_track_index; /* 0x80165fd8 */

/* BATTLE pointer slot 0x80173ca4; WORLD pointer slot 0x801cd75c. */
extern event_file_block_t* g_battle_event_block;
extern u16 g_battle_event_effect_target_misc_id; /* 0x80165ff8 */

/* 0x80166000: consumer passes mode 2 when equal to 2, otherwise mode 0.
 * The last Effect operand is not a second Y coordinate. */
extern s16 g_battle_event_effect_target_mode;
extern u16 g_battle_event_map_command_80_arg1;              /* 0x80173c94 */
extern u16 g_battle_event_map_command_80_arg2;              /* 0x80173c96 */
extern u16 g_battle_event_map_command_83_arg1;              /* 0x80174058 */
extern u16 g_battle_event_map_command_83_arg2;              /* 0x8017405a */
extern u16 g_battle_event_music_switch_fade;                /* 0x80173f56; consumer multiplies by 4 */
extern u16 g_battle_event_music_switch_volume;              /* 0x80173f54; scaled 0..96 to 0..127 */
extern s16 g_battle_event_pending_effect_id;                /* 0x80173cb4 */
extern s32 g_battle_event_pending_evtchr_save_slot;         /* 0x80173f4c */
extern s32 g_battle_event_pending_loaded_evtchr_clear_slot; /* 0x80173ca0 */
extern s16 g_battle_event_pending_map_jump_out_2_id;        /* 0x8017405c */

/* Deferred BATTLE commands: producer 0x80143bd8, consumer
 * 0x80143418..0x801439c0. The outer update at 0x80142d58 clears one-shot
 * requests, schedules script threads, then applies commands on its normal
 * path. These are separate globals, not a contiguous struct. Signed pending
 * slots use -1; request bits and polled states do not. */
extern s16 g_battle_event_pending_map_jump_out_id;            /* 0x80174054 */
extern s16 g_battle_event_pending_map_state;                  /* 0x80173f64; script variable 0x24 */
extern s32 g_battle_event_pending_reserved_vram_release_slot; /* 0x80173f48 */
extern s32 g_battle_event_pending_saved_evtchr_clear_slot;    /* 0x80173f50 */

/* 0x80173cac: packed misc ID << 8 | reserved VRAM slot; -1 absent. */
extern s32 g_battle_event_pending_unit_vram_copy;
extern s32 g_battle_event_speed;
extern s16 g_battle_event_unit_slots[4];

/* 0x80173f68: packed weather flags; consumer replaces it with a table index. */
extern s16 g_battle_event_weather_request;
extern u16 g_battle_field_object_use_request; /* 0x80165fe4 */
extern u16 g_battle_field_object_wait_status; /* 0x80166070 */
extern u16 g_battle_map_append_state;         /* 0x80165fe0 */

/* 1 starts, 2 polls, >=3 is reset to zero by the outer update. */
extern u16 g_battle_map_reload_state; /* 0x80165fde */

/* SwitchTrack's first operand is a trigger, not the selected track ID.
 * The consumer toggles the zero-based index, then uses main music slot 1/2. */
extern s16 g_battle_music_switch_request; /* 0x80165fc8 */
/* u16: the only loads of these three in the tree are in
 * attack_sound_resync_scenario_music_and_apply_map_darkness, which the target
 * reaches with lhu (that file's own comment records the andi a u16 merge would
 * add). Every other use is a store, which is sign-agnostic. */
extern u16 g_battle_music_track_1_id;          /* 0x80165fd4 */
extern u16 g_battle_music_track_2_id;          /* 0x80165fd6 */
extern s16 g_battle_music_unload_slot_request; /* 0x80165fca */

/* 0x80165fc0: -1 absent; volume = low 16 bits,
 * duration = (packed >> 14) & 0x3ffc. Not a linear duration field. */
extern s32 g_battle_music_volume_transition_request;
extern s32 g_battle_screen_fade;

/* Operand-byte counts, excluding the opcode; not per-operand width schemas.
 * BATTLE 0x8014d170, WORLD 0x8013a454. */
extern u8 g_battle_script_event_instruction_sizes[EVENT_OPCODE_COUNT];

/* 0x801660a3: suppress recursive write filtering while getters use operand 0. */
extern u8 g_battle_script_variable_write_guard;

/* 0x8016604e: result of battle_gfx_poll_unit_graphics_load, 0 done / 1 pending. */
extern u16 g_battle_unit_graphics_load_pending;
extern event_file_block_t g_event_script_buffer[];
extern s16 g_world_event_effect_target_misc_id;
extern s16 g_world_event_effect_target_mode;
extern u8 g_world_event_instruction_sizes[EVENT_OPCODE_COUNT];
/* Operand pairs that one world_script_execute_event opcode stores together;
 * each second halfword sits off a word boundary in WORLD bss, so each pair is
 * one object. */
typedef struct world_event_command_args {
    s16 arg1;
    s16 arg2;
} world_event_command_args_t;
typedef struct world_event_music_switch {
    s16 volume;
    s16 fade;
} world_event_music_switch_t;
extern world_event_command_args_t g_world_event_map_command_80_args;
extern world_event_command_args_t g_world_event_map_command_83_args;
extern world_event_music_switch_t g_world_event_music_switch;
extern s16 g_world_event_pending_effect_id;
extern s32 g_world_event_pending_evtchr_save_slot;
extern s32 g_world_event_pending_loaded_evtchr_clear_slot;
extern s16 g_world_event_pending_map_jump_out_2_id;
extern s16 g_world_event_pending_map_jump_out_id;
extern s16 g_world_event_pending_map_state;
extern s32 g_world_event_pending_reserved_vram_release_slot;
extern s32 g_world_event_pending_saved_evtchr_clear_slot;
extern s32 g_world_event_pending_unit_vram_copy;
extern const u8* g_world_event_script;
extern s16 g_world_event_unit_slots[4];
extern u8 g_world_event_variable_write_guard; /* WORLD counterpart: 0x80153387 */
extern s16 g_world_event_weather_request;

/* block */
void battle_block_start_thread(void);

/* camera */
void battle_camera_fusion_thread(void);
void battle_camera_wait_until_idle(void);
void battle_camera_thread(void);

/* dismiss */
void battle_dismiss_unit_event_instruction(s32 unit_id);

/* gfx */
void battle_gfx_load_evtchr_thread(void);

/* 0x8008d104..0x8008d138: drains immediate loader steps (2), returns
 * 0 when finished or 1 while work remains pending (including allocation retry). */
s32 battle_gfx_poll_unit_graphics_load(void);

/* map */
void battle_map_light_thread(void);

/* noop */
void battle_noop_80133150(s32 unused_unit_id);
void battle_noop_80149be4(const u8* unused_parameters);

/* script */
s32 battle_script_add_ghost_unit_event_instruction(
    s32 map_x, s32 map_y, s32 map_level, u16 map_height, s32 portrait_id, s32 misc_id, s32 flags);

/* Scenario interpreter interfaces shared by battle_script_execute_event and
 * world_script_execute_event. Signatures are as the interpreters bind them;
 * unverified names stay provisional. */
void battle_script_add_unit_start_thread(void);
s32 battle_script_check_unit_moving_event_instruction(s32 misc_id);
void battle_script_color_screen_thread(void);
void battle_script_color_unit_event_instruction(u8* ptr);
void battle_script_toggle_message_portrait_flip(u8* ptr);
void battle_script_set_units_palette_update_suppression(s32 unit_id, s32 enable);
void battle_script_apply_relative_camera(u8* p, s32* src);
void battle_script_load_attack_graphics_event_instruction(void);
void battle_script_execute_display_conditions_instruction(void);

/* Whole matching interpreter ranges:
 * BATTLE 0x80143bd8..0x80145f78, WORLD 0x800f6f20..0x800f92a0 (exclusive). */
void battle_script_execute_event(void);
s32 battle_script_find_instruction_byte_offset(s32 offset, s32 opcode);

/* 0x80149d6c..0x80149ebc: returns offset after the matching target marker;
 * missing targets stop the current thread. alternate_opcode accepts -1. */
s32 battle_script_find_jump_target(s32 limit_offset, event_opcode_e target_opcode, s32 alternate_opcode, s32 target_id);
s32 battle_script_get_rand16(void);
s32 battle_script_get_variable_bit_position(s32 variable_id);

/* Encoded variable IDs select words, bits, or nibbles. The command runner's
 * fourth argument is supplied by existing callers but is not read. */
s32* battle_script_get_variable_word_pointer_from_id(s32 variable_id);
void battle_script_inflict_status_thread(void);
s32 battle_script_is_tutorial_event_slot(void);
void battle_script_load_portrait_colors_event_instruction(s32 portrait_id);
void battle_script_mirrorsprite_event_instruction(u8* parameters);
void battle_script_pause_event_instruction(void);
void battle_script_play_effect_thread(void);
void battle_script_resetpalette_event_instruction(const u8* parameters);
void battle_script_run_condition(event_opcode_e opcode);
void battle_script_run_variable_command(s32 opcode, s32 destination_id, s32 source, s32 unused);
void battle_script_set_event_speed(s32 speed);
void battle_script_switch_tutorial_thread_for_event_instructions(void);
void battle_script_teleportin_event_instruction(s32 unit_id, s32 unused);
void battle_script_teleportout_event_instruction(s32 arg, s32 remove);
void battle_script_unit_animation_rotate_event_instruction(const u8* parameters);
void battle_script_unlockdate_event_instruction(s32 bitset, s32 date_index, s32 month, s32 day);
void battle_script_wait_value_event_instruction(u8* parameters);
void battle_script_waitrotateunit_and_waitrotateall_event_instruction(s32 unit_id);
void battle_script_waitspritemove_event_instruction(s32 unit_id);
void battle_script_waitwalk_event_instruction(s32 unit_id);
void battle_script_walk_to_thread(const event_walk_to_parameters_t* parameters);

/* sound */
void battle_sound_bg_thread(void);
void battle_sound_edit_bg_thread(void);

/* step */
/* 0x80088904: 0 finished, 1 pending/deferred, 2 immediate progress. */
s32 battle_gfx_step_queued_unit_graphics_load(void);

/* condition */
/* REQUIRE 0x801cafd4, invoked by both BATTLE and WORLD event dispatch. */
void require_condition_dispatch(void);

/* block */
void world_block_start_thread(void);

/* camera */
void world_camera_fusion_thread(void);
void world_camera_run_move_thread(void);
void world_camera_thread(void);

/* get */
s32 world_get_misc_id(s32 unit_id);
s32 world_get_script_variable_bit_position(s32 variable_id);
s32 world_unit_get_battle_index_by_entd_id(s32 entd_unit_id);

/* gfx */
void world_gfx_load_evtchr_thread(void);
void world_gfx_refresh_script_unit_environment_palette(const u8* parameters);

/* map */
void world_map_light_thread(void);

/* noop */
void world_noop_800e7808(s32 unused_unit_id);
void world_noop_800fd074(const u8* unused_parameters);

/* script */
void world_script_add_unit_start_thread(void);
s32 world_script_check_tutorial_event_slot(void);
void world_script_color_screen_thread(void);
void world_script_dismiss_unit_event_instruction(s32);
void world_script_execute_display_conditions_instruction(void);
void world_script_execute_event(void);
s32 world_script_find_instruction_byte_offset(s32 offset, s32 instruction);
s32 world_script_find_jump_target(s32, s32, s32, s32);
u32 world_script_get_random_u16(void);
s32* world_script_get_variable_word(s32 variable_id);
void world_script_inflict_status_thread(void);
s16 world_script_load_halfword(const u8* source);
void world_script_load_portrait_colors_event_instruction(s32);
void world_script_mirrorsprite_event_instruction(const u8* parameters);
void world_script_pause_event_instruction(void);
void world_script_play_effect_thread(void);
void world_script_run_condition(event_opcode_e opcode);
void world_script_run_variable_command(s32 opcode, s32 destination_id, s32 source, s32 unused);
void world_script_teleport_unit_in(s32 unit_id, s32 unused);
void world_script_unlockdate_event_instruction(u32* date_bits, s32 index, s32 month, s32 day);
void world_script_waitrotateunit_and_waitrotateall_event_instruction(s32);
void world_script_waitspritemove_event_instruction(s32);
void world_script_waitwalk_event_instruction(s32 unit_id);

/* sound */
void world_sound_bg_thread(void);
void world_sound_edit_bg_thread(void);

/* text */
void world_text_character_handling_thread(void);

extern s32 g_battle_event_frame_counter;

#endif
