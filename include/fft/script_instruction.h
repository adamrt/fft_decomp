#ifndef SCRIPT_INSTRUCTION_H
#define SCRIPT_INSTRUCTION_H

/* Event script bytecode: the opcodes, their operand values, and the event file, instruction and operand records. */

#include "fft/gfx.h"
#include "psx/types.h"

/* block sizes and unit selectors */
enum {
    EVENT_BLOCK_BYTES = 8192,
    EVENT_BLOCK_SECTORS = 4,
    EVENT_OPCODE_COUNT = 256,
    /* External unit-id selectors used by scenario commands. */
    EVENT_UNIT_SELECTOR_SLOT_FIRST = 0x64,
    EVENT_UNIT_SELECTOR_SLOT_COUNT = 4,
    /* The first ENTD unit whose team flags have a nonzero UNIT_TEAM_IMPORTANCE_MASK. */
    EVENT_UNIT_SELECTOR_IMPORTANT_UNIT = 0x68,
    EVENT_UNIT_SELECTOR_ACTIVE_TURN = 0x69,
    /* First unit-group selector. The instructions that take a unit group pass
     * selector - 0xfe to script_filter_unit_id_by_mode as its mode (2 up);
     * unit id 0 selects mode 1, every unit. */
    EVENT_UNIT_GROUP_FIRST = 0x100,
    /* Five ordinary deployed-unit IDs; the end value is exclusive. */
    EVENT_UNIT_ID_DEPLOYED_FIRST = 0x78,
    EVENT_UNIT_ID_DEPLOYED_END = 0x7d,
    /* script_resolve_runtime_id result for an absent unit. */
    EVENT_RUNTIME_ID_NONE = 2000,
    /* unit_get_battle_id_by_event_unit_id result for an absent unit: the same value. */
    EVENT_BATTLE_ID_NONE = EVENT_RUNTIME_ID_NONE,
    /* g_script_unit_slots[] entry with no ghost unit assigned. */
    EVENT_UNIT_SLOT_EMPTY = 1000,
};

/* opcodes */
/* Retail scenario bytecode, reviewed against BATTLE's script_execute_event and
 * the shared BATTLE/WORLD operand-length table g_script_event_instruction_sizes.
 *
 * Encoding: one opcode byte, then the operand bytes the length table counts
 * for that opcode. 16-bit operands are little-endian and unaligned. After a
 * handler returns, the interpreter steps over 1 + length bytes.
 * JUMP_FORWARD, JUMP_FORWARD_IF_ZERO and JUMP_BACK seek past their target
 * marker instead; BLOCK_START, ADD_UNIT_START and CAMERA_FUSION_START hand
 * the instructions that follow to a new thread and seek to the matching END.
 * A byte with no dispatch case ends the script.
 *
 * Each comment gives the encoded operand byte count, then the operands in the
 * order the handler reads them. A plain name is one unsigned byte, (s8) a
 * signed byte, (u16) or (s16) a 16-bit value, and "unread" a byte no handler
 * reads. A "unit id" names one unit (script_resolve_runtime_id); a "unit group"
 * is a unit id, 0 for every unit, or a team selector from
 * EVENT_UNIT_GROUP_FIRST (script_prepare_unit_filter).
 *
 * UNKNOWN values are explicit dispatch cases, not guesses for otherwise
 * unrecognized byte values. UNHANDLED values have a length in the table but
 * no dispatch case in either interpreter: the scanners step over them by that
 * length and the interpreters end the script on them. Established command
 * labels describe BATTLE; WORLD can handle them differently: there WALK_TO
 * does nothing outside a block, ARROW resolves both units and stops,
 * DARK_SCREEN's thread only sets its task id, and the two CHANGE_MAP
 * instructions leave the script light transform alone. BETA does not imply
 * unused. JUMP animates a unit, whereas JUMP_FORWARD/JUMP_BACK branch within
 * the script. CALL_FUNCTION selects a built-in operation, not a native
 * function pointer. EFFECT_END starts the staged effect; it does not cancel
 * it. */
typedef enum event_opcode {
    /* 14: thread slot, dialog type, text id (u16), unit id (u16), portrait + 1, x offset (s16), y offset (s16),
     * arrow offset (s16), window options */
    EVENT_OPCODE_DISPLAY_MESSAGE = 0x10,
    /* 5: unit group (u16), animation id (u16), facing-update suppression (0 clears it, 1 sets it) */
    EVENT_OPCODE_UNIT_ANIM = 0x11,
    EVENT_OPCODE_WAIT_UNIT_READY = 0x12, /* 2: unit id (u16); the readiness test always passes */
    EVENT_OPCODE_CHANGE_MAP_BETA = 0x13, /* 2: map id (u16) */
    EVENT_OPCODE_UNHANDLED_14 = 0x14,    /* 2 */
    EVENT_OPCODE_PAUSE = 0x16,           /* 0; waits for Select, Cross or Circle */
    EVENT_OPCODE_UNKNOWN_17 = 0x17,      /* 5: unread; recognized no-op */
    /* 6: effect id (u16), target unit id, x, y, target mode (2: the target is also the caster); x and y are stored
     * and not read again */
    EVENT_OPCODE_EFFECT = 0x18,
    /* 16: x, z, y, tilt, yaw, roll, zoom, frames (s16 each); 10000 keeps a component */
    EVENT_OPCODE_CAMERA = 0x19,
    EVENT_OPCODE_MAP_DARKNESS = 0x1a, /* 5: blend mode, red, green, blue (s8 each), duration */
    /* 14: light angle x, y, z, light scale x, y, z, frames (s16 each) */
    EVENT_OPCODE_MAP_LIGHT = 0x1b,
    EVENT_OPCODE_EVENT_SPEED = 0x1c,         /* 1: speed */
    EVENT_OPCODE_CAMERA_FUSION_START = 0x1d, /* 0; seeks to CAMERA_FUSION_END */
    EVENT_OPCODE_CAMERA_FUSION_END = 0x1e,   /* 0 */
    /* 5: unit id (u16), unit id (u16), keep-yaw flag; rewrites the next CAMERA's x, z, y and yaw */
    EVENT_OPCODE_FOCUS = 0x1f,
    EVENT_OPCODE_UNHANDLED_20 = 0x20,          /* 6 */
    EVENT_OPCODE_SOUND_EFFECT = 0x21,          /* 2: sound effect id (u16) */
    EVENT_OPCODE_SWITCH_TRACK = 0x22,          /* 3: request (any value), volume, fade length */
    EVENT_OPCODE_UNKNOWN_23 = 0x23,            /* 3: unread; recognized no-op */
    EVENT_OPCODE_UNKNOWN_24 = 0x24,            /* 0; recognized no-op */
    EVENT_OPCODE_SET_SPEAKER_PAIR = 0x25,      /* 4: unit id (u16), unit id (u16) */
    EVENT_OPCODE_RESUME_WAITING_DIALOG = 0x26, /* 2: text id - 1 (u16) */
    EVENT_OPCODE_RELOAD_MAP_STATE = 0x27,      /* 0 */
    /* 8: unit id (u16), x, y, elevation, speed (s16), terrain-cost flag (event_walk_to_parameters_t) */
    EVENT_OPCODE_WALK_TO = 0x28,
    EVENT_OPCODE_WAIT_WALK = 0x29,   /* 2: unit id (u16) */
    EVENT_OPCODE_BLOCK_START = 0x2a, /* 0; seeks to BLOCK_END */
    EVENT_OPCODE_BLOCK_END = 0x2b,   /* 0 */
    /* 7: unit id (u16), unit group (u16), rotate mode, speed index, delay; the unit and the group turn to face each
     * other */
    EVENT_OPCODE_FACE_EACH_OTHER = 0x2c,
    /* 6: unit group (u16), facing code (event_rotate_unit_facing_e), rotate mode, speed index, delay */
    EVENT_OPCODE_ROTATE_UNIT = 0x2d,
    /* 8: top red, green, blue, bottom red, green, blue, frames, store flag (0 also stores the gradient) */
    EVENT_OPCODE_BACKGROUND = 0x2e,
    EVENT_OPCODE_COLOR_BG_BETA = 0x31, /* 5: blend mode, red, green, blue (s8 each), frames */
    /* 7: unit group (u16), blend mode, red, green, blue (s8 each), time */
    EVENT_OPCODE_COLOR_UNIT = 0x32,
    EVENT_OPCODE_COLOR_FIELD = 0x33,            /* 5: blend mode, red, green, blue (s8 each), step */
    EVENT_OPCODE_UNHANDLED_34 = 0x34,           /* 2 */
    EVENT_OPCODE_UNHANDLED_35 = 0x35,           /* 4 */
    EVENT_OPCODE_UNHANDLED_36 = 0x36,           /* 2 */
    EVENT_OPCODE_FLIP_MESSAGE_PORTRAIT = 0x37,  /* 2: text id - 1 (s16) */
    EVENT_OPCODE_FOCUS_SPEED = 0x38,            /* 2: speed (s16); rewrites the next CAMERA's frames */
    EVENT_OPCODE_WAIT_ALL_UNIT_MOVEMENT = 0x39, /* 0 */
    EVENT_OPCODE_WAIT_FILE_LOAD = 0x3a,         /* 0 */
    /* 12: unit id (u16), offset x, y, z (s16 each), curve, curve weight, frames (u16) */
    EVENT_OPCODE_SPRITE_MOVE = 0x3b,
    EVENT_OPCODE_WEATHER = 0x3c,     /* 2: (u16) weather id in bits 0-3, apply flag in bits 8-11 */
    EVENT_OPCODE_REMOVE_UNIT = 0x3d, /* 2: unit id (u16) */
    /* 9: mode, start red, green, blue, end red, green, blue, frames (u16) */
    EVENT_OPCODE_COLOR_SCREEN = 0x3e,
    EVENT_OPCODE_SET_TILE_CAMERA_BLOCK = 0x3f, /* 4: x, y, layer, flat camera-block mask */
    EVENT_OPCODE_SET_TILE_HEIGHT = 0x40,       /* 5: x, y, layer, height, untargetable flag */
    EVENT_OPCODE_EARTHQUAKE_START = 0x41,      /* 4: shake step, period, second shake step, second period */
    EVENT_OPCODE_EARTHQUAKE_END = 0x42,        /* 0 */
    EVENT_OPCODE_CALL_FUNCTION = 0x43,         /* 1: event_call_function_e */
    EVENT_OPCODE_DRAW = 0x44,                  /* 2: unit id (u16) */
    EVENT_OPCODE_ADD_UNIT = 0x45,              /* 3: ENTD unit id (u16), hidden flag */
    EVENT_OPCODE_ERASE = 0x46,                 /* 2: unit id (u16) */
    /* 8: spritesheet id (u16), event unit slot, x, y, elevation, facing, hidden flag */
    EVENT_OPCODE_ADD_GHOST_UNIT = 0x47,
    EVENT_OPCODE_WAIT_ADD_UNIT = 0x48,       /* 0 */
    EVENT_OPCODE_ADD_UNIT_START = 0x49,      /* 0; seeks to ADD_UNIT_END */
    EVENT_OPCODE_ADD_UNIT_END = 0x4a,        /* 0 */
    EVENT_OPCODE_WAIT_ADD_UNIT_END = 0x4b,   /* 0 */
    EVENT_OPCODE_CHANGE_MAP = 0x4c,          /* 2: map id (u16) */
    EVENT_OPCODE_REQUEST_MAP_JUMP_IN = 0x4d, /* 1: duration */
    EVENT_OPCODE_UNIT_SHADOW = 0x4e,         /* 3: unit id (u16), flag (0 sets the shadow trigger, else clears it) */
    EVENT_OPCODE_SET_DAYTIME = 0x4f,         /* 1: time of day */
    EVENT_OPCODE_PORTRAIT_COL = 0x50,        /* 1: index of the four-sector portrait block to load */
    EVENT_OPCODE_CHANGE_DIALOG = 0x51,       /* 5: dialog selector, text id (u16; 0xffff closes), portrait + 1 (u16) */
    EVENT_OPCODE_UNHANDLED_52 = 0x52,        /* 2 */
    /* 7: unit id (u16), unit group (u16), rotate mode, speed index, delay; the group turns to face the unit */
    EVENT_OPCODE_FACE_UNIT = 0x53,
    EVENT_OPCODE_USE_3D_OBJECT = 0x54,     /* 2: object id, state */
    EVENT_OPCODE_USE_FIELD_OBJECT = 0x55,  /* 2: texture animation id, value */
    EVENT_OPCODE_WAIT_3D_OBJECT = 0x56,    /* 0 */
    EVENT_OPCODE_WAIT_FIELD_OBJECT = 0x57, /* 0 */
    EVENT_OPCODE_LOAD_EVTCHR = 0x58,       /* 3: slot, EVTCHR.BIN block (u16) */
    EVENT_OPCODE_SAVE_EVTCHR = 0x59,       /* 1: slot */
    EVENT_OPCODE_SAVE_EVTCHR_CLEAR = 0x5a, /* 1: slot */
    EVENT_OPCODE_LOAD_EVTCHR_CLEAR = 0x5b, /* 1: slot */
    /* The next two are deferred requests; the copy can be refused, and the release does not erase pixels. */
    /* 3: unit id (u16), slot */
    EVENT_OPCODE_COPY_UNIT_GRAPHICS_TO_RESERVED_VRAM_SLOT = 0x5c,
    EVENT_OPCODE_RELEASE_RESERVED_VRAM_SLOT = 0x5d, /* 1: slot */
    EVENT_OPCODE_END_TRACK = 0x5e,                  /* 1: scenario music slot to unload */
    EVENT_OPCODE_WARP_UNIT = 0x5f,                  /* 6: ENTD unit id (u16), x, y, elevation, facing */
    EVENT_OPCODE_FADE_SOUND = 0x60,                 /* 2: left shift applied to the frames, frames */
    EVENT_OPCODE_UNKNOWN_61 = 0x61,                 /* 3: unread; recognized no-op */
    EVENT_OPCODE_SET_BATTLE_UNIT_POSITION = 0x62,   /* 6: battle unit slot, unread, x, y, elevation, facing */
    /* 1: interpolation mode in bits 0-1, component group left linear in bits 2-3, easing weight in bits 4-7 */
    EVENT_OPCODE_CAMERA_SPEED_CURVE = 0x63,
    EVENT_OPCODE_WAIT_ROTATE_UNIT = 0x64,           /* 2: unit id (u16) */
    EVENT_OPCODE_WAIT_ROTATE_ALL = 0x65,            /* 0 */
    EVENT_OPCODE_STORE_MODIFIED_MAP_PALETTE = 0x66, /* 0 */
    EVENT_OPCODE_UNHANDLED_67 = 0x67,               /* 2 */
    EVENT_OPCODE_MIRROR_SPRITE = 0x68,              /* 3: unit id (u16), flag (1 mirrors, else restores) */
    /* 8: unit group (u16), x, y, unread, rotate mode, speed index, delay */
    EVENT_OPCODE_FACE_TILE = 0x69,
    /* The next two ramp main_sound_set_sfx_echo's value from the start level to the end level, one step a frame. */
    EVENT_OPCODE_EDIT_BG_SOUND = 0x6a, /* 5: sound id, start level, end level, unread, frames */
    EVENT_OPCODE_BG_SOUND = 0x6b,      /* 5: sound id, start level, end level, find-channel flag, frames */
    EVENT_OPCODE_RESUME_UNIT_PALETTE_UPDATES = 0x6c,   /* 2: unit group (u16) */
    EVENT_OPCODE_SUPPRESS_UNIT_PALETTE_UPDATES = 0x6d, /* 2: unit group (u16) */
    /* 12: unit id (u16), offset x, y, z (s16 each), curve, curve weight, speed (u16) */
    EVENT_OPCODE_SPRITE_MOVE_BETA = 0x6e,
    EVENT_OPCODE_WAIT_SPRITE_MOVE = 0x6f, /* 2: unit id (u16) */
    EVENT_OPCODE_JUMP = 0x70,             /* 4: unit id (u16), jump length + 1, direction */
    EVENT_OPCODE_SET_FIRST_UNIT = 0x71,   /* 2: unit id (u16) */
    EVENT_OPCODE_DEBUG_PAUSE = 0x72,      /* 0 */
    /* 14: x, z, y, tilt, yaw, roll, zoom deltas (s16 each; 10000 keeps a component); rewrites the next CAMERA */
    EVENT_OPCODE_RELATIVE_CAMERA = 0x73,
    EVENT_OPCODE_UNKNOWN_74 = 0x74, /* 6: unread; calls an empty function */
    EVENT_OPCODE_UNKNOWN_75 = 0x75, /* 6: unread; recognized no-op */
    /* 6: unread, burst mode, delay divisor, angle step (u16), radius step */
    EVENT_OPCODE_DARK_SCREEN = 0x76,
    EVENT_OPCODE_REMOVE_DARK_SCREEN = 0x77,   /* 0 */
    EVENT_OPCODE_DISPLAY_CONDITIONS = 0x78,   /* 2: event_display_condition_e, frames */
    EVENT_OPCODE_WALK_TO_ANIM = 0x79,         /* 4: unit id (u16), animation id (u16) */
    EVENT_OPCODE_DISMISS_UNIT = 0x7a,         /* 2: party sprite set (u16) */
    EVENT_OPCODE_TUTORIAL_BUTTON_WAIT = 0x7b, /* 2: pad button mask (u16) */
    EVENT_OPCODE_END_SOUND = 0x7c,            /* 0 */
    EVENT_OPCODE_SHOW_GRAPHIC = 0x7d,         /* 1: graphic id */
    EVENT_OPCODE_WAIT_VALUE = 0x7e,           /* 4: variable id (u16), value (s16) */
    EVENT_OPCODE_EVTCHR_PALETTE = 0x7f,       /* 4: unit id (u16), slot, CLUT index */
    EVENT_OPCODE_MARCH = 0x80,                /* 3: unit group (u16), frames to wait after each unit */
    /* 3: unit group (u16), flag (0 clears the suppression, else sets it) */
    EVENT_OPCODE_SET_UNIT_MOVEMENT_EFFECT_SUPPRESSION = 0x81,
    EVENT_OPCODE_LOAD_ATTACK_GRAPHICS = 0x82, /* 0 */
    EVENT_OPCODE_CHANGE_STATS = 0x83,         /* 5: unit group (u16), event_change_stats_e, delta (s16) */
    EVENT_OPCODE_PLAY_TUNE = 0x84,            /* 1: tune id */
    EVENT_OPCODE_UNLOCK_DATE = 0x85,          /* 1: date slot + 1; 0 does nothing */
    EVENT_OPCODE_TEMP_WEAPON = 0x86,          /* 3: unit id (u16), item id */
    EVENT_OPCODE_ARROW = 0x87,                /* 4: target unit id (u16), shooter unit id (u16) */
    EVENT_OPCODE_MAP_UNFREEZE = 0x88,         /* 0 */
    EVENT_OPCODE_MAP_FREEZE = 0x89,           /* 0 */
    EVENT_OPCODE_EFFECT_START = 0x8a,         /* 0 */
    EVENT_OPCODE_EFFECT_END = 0x8b,           /* 0 */
    /* 6: unit id (u16), facing, animation id (u16), unread; the facing-update suppression flag tested is the
     * animation id's high byte */
    EVENT_OPCODE_UNIT_ANIM_ROTATE = 0x8c,
    EVENT_OPCODE_WAIT_GRAPHIC_PRINT = 0x8e,          /* 0 */
    EVENT_OPCODE_LOCK_MENU_INPUT = 0x8f,             /* 1: lock mode */
    EVENT_OPCODE_TUTORIAL_COORDINATE_CONFIRM = 0x90, /* 3: cursor x, cursor y, unread */
    EVENT_OPCODE_SHOW_MAP_TITLE = 0x91,              /* 3: x offset, y offset, step (s8 each) */
    EVENT_OPCODE_INFLICT_STATUS = 0x92,              /* 5: unit id (u16), event_inflict_status_e, frames (u16) */
    EVENT_OPCODE_SET_DATE_ADVANCE = 0x93,            /* 2: value (s16) */
    EVENT_OPCODE_TELEPORT_OUT = 0x94,                /* 2: unit id (u16) */
    EVENT_OPCODE_UNKNOWN_95 = 0x95,                  /* 0; recognized no-op */
    EVENT_OPCODE_APPEND_MAP_STATE = 0x96,            /* 0 */
    EVENT_OPCODE_RESET_PALETTE = 0x97,               /* 2: unit id (u16) */
    EVENT_OPCODE_TELEPORT_IN = 0x98,                 /* 2: unit id (u16) */
    EVENT_OPCODE_BLUE_REMOVE_UNIT = 0x99,            /* 2: ENTD unit id (u16) */

    /* Compare script operands 0 and 1; write the boolean to operand 0. */
    EVENT_CONDITION_SECOND_GTE_FIRST = 0xa0, /* 0 */
    EVENT_CONDITION_FIRST_GTE_SECOND = 0xa1, /* 0 */
    EVENT_CONDITION_EQUAL = 0xa2,            /* 0 */
    EVENT_CONDITION_NOT_EQUAL = 0xa3,        /* 0 */
    EVENT_CONDITION_FIRST_LT_SECOND = 0xa4,  /* 0 */
    EVENT_CONDITION_SECOND_LT_FIRST = 0xa5,  /* 0 */

    /* Update a destination variable using an immediate or variable value.
     * ZERO_VARIABLE only encodes the destination. */
    EVENT_OPCODE_ADD_IMMEDIATE = 0xb0,      /* 4: variable id (u16), value (u16) */
    EVENT_OPCODE_ADD_VARIABLE = 0xb1,       /* 4: variable id (u16), source variable id (u16) */
    EVENT_OPCODE_SUBTRACT_IMMEDIATE = 0xb2, /* 4: variable id (u16), value (u16) */
    EVENT_OPCODE_SUBTRACT_VARIABLE = 0xb3,  /* 4: variable id (u16), source variable id (u16) */
    EVENT_OPCODE_MULTIPLY_IMMEDIATE = 0xb4, /* 4: variable id (u16), value (u16) */
    EVENT_OPCODE_MULTIPLY_VARIABLE = 0xb5,  /* 4: variable id (u16), source variable id (u16) */
    EVENT_OPCODE_DIVIDE_IMMEDIATE = 0xb6,   /* 4: variable id (u16), value (u16) */
    EVENT_OPCODE_DIVIDE_VARIABLE = 0xb7,    /* 4: variable id (u16), source variable id (u16) */
    EVENT_OPCODE_MODULO_IMMEDIATE = 0xb8,   /* 4: variable id (u16), value (u16) */
    EVENT_OPCODE_MODULO_VARIABLE = 0xb9,    /* 4: variable id (u16), source variable id (u16) */
    EVENT_OPCODE_AND_IMMEDIATE = 0xba,      /* 4: variable id (u16), value (u16) */
    EVENT_OPCODE_AND_VARIABLE = 0xbb,       /* 4: variable id (u16), source variable id (u16) */
    EVENT_OPCODE_OR_IMMEDIATE = 0xbc,       /* 4: variable id (u16), value (u16) */
    EVENT_OPCODE_OR_VARIABLE = 0xbd,        /* 4: variable id (u16), source variable id (u16) */
    EVENT_OPCODE_ZERO_VARIABLE = 0xbe,      /* 2: variable id (u16) */

    /* 16: unread; no-op that FOCUS writes over the CAMERA opcode it cannot aim (a unit is absent) */
    EVENT_OPCODE_CAMERA_DISABLED = 0xc0,
    EVENT_OPCODE_JUMP_FORWARD_IF_ZERO = 0xd0, /* 1: target id; taken when the comparison result is 0 */
    EVENT_OPCODE_JUMP_FORWARD = 0xd1,         /* 1: target id */
    EVENT_OPCODE_FORWARD_TARGET = 0xd2,       /* 1: target id */
    EVENT_OPCODE_JUMP_BACK = 0xd3,            /* 1: target id */
    /* 1: target id; a JUMP_FORWARD_IF_ZERO target that ends the script when run */
    EVENT_OPCODE_FORWARD_TARGET_EXIT = 0xd4,
    EVENT_OPCODE_BACK_TARGET = 0xd5,          /* 1: target id */
    EVENT_OPCODE_UNKNOWN_D8 = 0xd8,           /* 1: unread; recognized no-op */
    EVENT_OPCODE_BACK_TARGET_BARRIER = 0xd9,  /* 1: unread; JUMP_BACK ignores the targets before it */
    EVENT_OPCODE_UNKNOWN_DA = 0xda,           /* 0; recognized no-op */
    EVENT_OPCODE_END = 0xdb,                  /* 0 */
    EVENT_OPCODE_UNKNOWN_DC = 0xdc,           /* 0; stores 13 to a 32-bit value nothing else reads */
    EVENT_OPCODE_UNKNOWN_E0 = 0xe0,           /* 2: unread; recognized no-op */
    EVENT_OPCODE_UNHANDLED_E1 = 0xe1,         /* 1 */
    EVENT_OPCODE_UNHANDLED_E2 = 0xe2,         /* 1 */
    EVENT_OPCODE_END_2 = 0xe3,                /* 0 */
    EVENT_OPCODE_UNHANDLED_E4 = 0xe4,         /* 1 */
    EVENT_OPCODE_WAIT_FOR_INSTRUCTION = 0xe5, /* 2: thread_task_e (u16); waits until no thread runs it */
    EVENT_OPCODE_YIELD = 0xf0,                /* 0 */
    EVENT_OPCODE_WAIT = 0xf1,                 /* 2: frames (s16) */
    EVENT_OPCODE_NOP = 0xf2,                  /* 0 */
} event_opcode_e;

/* operand values */
/* Built-in operation selected by EVENT_OPCODE_CALL_FUNCTION. The PSX domain
 * is 0x01..0x12 and is shared by the structurally parallel BATTLE and WORLD
 * interpreters. BLOCK_START handles only the mute/unmute selectors. */
typedef enum event_call_function {
    EVENT_CALL_FUNCTION_JOIN_UNITS_SILENTLY = 0x01,
    EVENT_CALL_FUNCTION_ADVANCE_YEAR = 0x02,
    EVENT_CALL_FUNCTION_INCREASE_BLUE_TEAM_BRAVE = 0x03,
    EVENT_CALL_FUNCTION_REMOVE_ALL_ENEMY_UNITS = 0x04,
    EVENT_CALL_FUNCTION_REQUEST_CAMERA_YAW_WRAP = 0x05,
    EVENT_CALL_FUNCTION_OPEN_SAVE_MENU = 0x06,
    EVENT_CALL_FUNCTION_ADVANCE_YEAR_ALTERNATE = 0x07,
    EVENT_CALL_FUNCTION_DISABLE_DEBUG_DISPLAY = 0x08,
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

/* Display selected by the first EVENT_OPCODE_DISPLAY_CONDITIONS operand and
 * run by require_condition_dispatch. Values from GENERAL_FIRST up show bonus
 * image (value - GENERAL_FIRST) as a banner. */
typedef enum event_display_condition {
    EVENT_DISPLAY_CONDITION_SPECIAL_CASES = 0x00,
    EVENT_DISPLAY_CONDITION_SHOW_READY_BUGGED = 0x01,
    EVENT_DISPLAY_CONDITION_BATTLE_CONGRATULATIONS = 0x02,
    EVENT_DISPLAY_CONDITION_GIL_REWARD = 0x03,
    EVENT_DISPLAY_CONDITION_WAR_TROPHIES = 0x04,
    EVENT_DISPLAY_CONDITION_APPLY_PERMANENT_BRAVE_FAITH_CHANGES = 0x05,
    EVENT_DISPLAY_CONDITION_REMOVE_LOW_BRAVE_HIGH_FAITH_UNITS = 0x06,
    EVENT_DISPLAY_CONDITION_APPLY_POST_BATTLE_UNIT_CHANGES = 0x07,
    EVENT_DISPLAY_CONDITION_GENERAL_FIRST = 0x08,
} event_display_condition_e;

/* EVENT_OPCODE_ROTATE_UNIT facing codes; any other value is an absolute
 * facing. RELATIVE_FIRST and the two values after it add one to three quarter
 * turns to the unit's current facing. */
typedef enum event_rotate_unit_facing {
    EVENT_ROTATE_UNIT_FACING_CAMERA = 0x10,
    EVENT_ROTATE_UNIT_FACING_RELATIVE_FIRST = 0x11,
    EVENT_ROTATE_UNIT_FACING_RESTORE = 0x14,
} event_rotate_unit_facing_e;

/* First 16-bit value of a scenario condition record, as
 * script_check_scenario_condition tests it. The record's next 16-bit values are
 * called id and value below; the two tile conditions also read a y and an
 * elevation. Other types fail. */
typedef enum event_scenario_condition {
    EVENT_SCENARIO_CONDITION_VARIABLE_EQUAL = 1,    /* script variable id == value */
    EVENT_SCENARIO_CONDITION_VARIABLE_AT_LEAST = 2, /* script variable id >= value */
    EVENT_SCENARIO_CONDITION_VARIABLE_AT_MOST = 3,  /* script variable id <= value */
    EVENT_SCENARIO_CONDITION_UNIT_PRESENT = 4,      /* the unit with unit id `id` exists */
    EVENT_SCENARIO_CONDITION_UNIT_HP_AT_LEAST = 5,
    EVENT_SCENARIO_CONDITION_UNIT_HP_AT_MOST = 6,
    EVENT_SCENARIO_CONDITION_UNIT_HP_PERCENT_AT_LEAST = 7, /* hp * 100 / (max_hp + 1) >= value */
    EVENT_SCENARIO_CONDITION_UNIT_HP_PERCENT_AT_MOST = 8,
    EVENT_SCENARIO_CONDITION_UNIT_MP_AT_LEAST = 9,
    EVENT_SCENARIO_CONDITION_UNIT_MP_AT_MOST = 10,
    /* The unit the check was started for has unit id `id`, or is a deployed
     * unit with that character identity. */
    EVENT_SCENARIO_CONDITION_ACTING_UNIT = 11,
    EVENT_SCENARIO_CONDITION_ITEM_EQUIPPED = 13, /* a unit with unit id 1, 2 or 3 has item `id` equipped */
    EVENT_SCENARIO_CONDITION_WAR_FUNDS_AT_LEAST = 14,
    EVENT_SCENARIO_CONDITION_WAR_FUNDS_AT_MOST = 15,
    EVENT_SCENARIO_CONDITION_DATE_ON_OR_BEFORE = 16,   /* month id, day value */
    EVENT_SCENARIO_CONDITION_DATE_ON_OR_AFTER = 17,    /* month id, day value */
    EVENT_SCENARIO_CONDITION_CASUALTIES_AT_LEAST = 18, /* compares with id, not value */
    EVENT_SCENARIO_CONDITION_CASUALTIES_AT_MOST = 19,  /* compares with id, not value */
    EVENT_SCENARIO_CONDITION_ENEMIES_DEFEATED = 22,    /* battle_action_check_battle_outcome returns 0 */
    EVENT_SCENARIO_CONDITION_UNIT_AT_TILE = 24,        /* unit id stands on x value, the y and the elevation */
    /* Stores id as the current event; the result is always RUN_EVENT. */
    EVENT_SCENARIO_CONDITION_RUN_EVENT = 25,
    /* A unit with sprite palette id stands on that tile. */
    EVENT_SCENARIO_CONDITION_PALETTE_UNIT_AT_TILE = 37,
} event_scenario_condition_e;

/* Result of script_check_scenario_condition for one condition record.
 * script_run_scenario_conditions walks a chain of records while they pass,
 * drops the chain at the first failure and reports an event to run when it
 * reaches the record (type 25) that stores the event id. */
typedef enum event_scenario_condition_result {
    EVENT_SCENARIO_CONDITION_RESULT_FAIL = 0,
    EVENT_SCENARIO_CONDITION_RESULT_PASS = 1,
    EVENT_SCENARIO_CONDITION_RESULT_RUN_EVENT = 2,
} event_scenario_condition_result_e;

/* Stat selected by EVENT_OPCODE_CHANGE_STATS; other values change nothing. */
typedef enum event_change_stats {
    EVENT_CHANGE_STATS_HP = 0x00,
    EVENT_CHANGE_STATS_CT = 0x02,
    EVENT_CHANGE_STATS_BRAVE = 0x03,
    EVENT_CHANGE_STATS_FAITH = 0x04,
} event_change_stats_e;

/* Operation selected by EVENT_OPCODE_INFLICT_STATUS; other values only wait. */
typedef enum event_inflict_status {
    /* A dead unit returns with 1 HP and Critical. */
    EVENT_INFLICT_STATUS_REVIVE = 0x00,
    EVENT_INFLICT_STATUS_CRYSTAL = 0x01,
    EVENT_INFLICT_STATUS_POISON = 0x02,
} event_inflict_status_e;

/* event file */
/* event_file_block_t.text_offset_or_marker once script_load_event has processed the block. */
#define EVENT_TEXT_OFFSET_PROCESSED 0xf2f2f2f2U

/* EVENT/TEST.EVT record. The first 32-bit value is relative to the block base, not
 * data[]. script_load_event converts it to a text pointer, then overwrites
 * it with EVENT_TEXT_OFFSET_PROCESSED. This is not a decoded instruction
 * array or a native scheduler context. The interpreter starts at byte zero:
 * the processed header is four one-byte 0xf2 NOPs before data[]. */
typedef struct event_file_block {
    u32 text_offset_or_marker;
    u8 data[EVENT_BLOCK_BYTES - 4];
} event_file_block_t;
typedef char event_file_block_size_must_be_8192[(sizeof(event_file_block_t) == EVENT_BLOCK_BYTES) ? 1 : -1];

/* dialog instructions */
/* Dialogue commands are byte-aligned records in the event instruction stream. */
typedef struct event_display_message_instruction {
    u8 opcode;
    u8 thread_id;
    u8 dialog_type;
    u8 text_id_le[2]; /* one-based text id */
    u8 unit_id_le[2];
    u8 portrait_selector;
    u8 x_offset_le[2];
    u8 y_offset_le[2];
    u8 arrow_offset_le[2];
    u8 window_options;
} event_display_message_instruction_t;
typedef char
    event_display_message_instruction_size_must_be_15[sizeof(event_display_message_instruction_t) == 15 ? 1 : -1];

typedef struct event_change_dialog_instruction {
    u8 opcode;
    u8 window_selector;
    u8 text_id_le[2]; /* one-based text id; 0xffff closes the window */
    u8 portrait_selector_le[2];
} event_change_dialog_instruction_t;
typedef char event_change_dialog_instruction_size_must_be_6[sizeof(event_change_dialog_instruction_t) == 6 ? 1 : -1];

static inline event_display_message_instruction_t* event_display_message_at(u8* base, s32 byte_offset) {
    return (event_display_message_instruction_t*)(base + byte_offset);
}

static inline event_change_dialog_instruction_t* event_change_dialog_at(u8* base, s32 byte_offset) {
    return (event_change_dialog_instruction_t*)(base + byte_offset);
}

/* unit operands */
/* Operand records start after the opcode byte. 16-bit values stay in
 * little-endian byte arrays because the interpreter may address them at any
 * byte alignment. */
typedef struct event_unit_parameters {
    u8 unit_id_le[2];
} event_unit_parameters_t;

typedef struct event_unit_placement {
    u8 x;
    u8 y;
    u8 elevation;
    u8 facing;
} event_unit_placement_t;

/* Direct battle-slot placement bypasses the renderer update performed by WarpUnit. */
typedef struct event_set_battle_unit_position_instruction {
    u8 opcode;
    u8 battle_id;
    u8 _unused_02;
    event_unit_placement_t placement;
} event_set_battle_unit_position_instruction_t;
typedef char event_set_battle_unit_position_instruction_size_must_be_7
    [sizeof(event_set_battle_unit_position_instruction_t) == 7 ? 1 : -1];

typedef struct event_warp_unit_parameters {
    u8 unit_id_le[2];
    event_unit_placement_t placement;
} event_warp_unit_parameters_t;

/* Eight-byte operand payload of EVENT_OPCODE_WALK_TO. Multi-byte values
 * remain byte arrays because event scripts are not guaranteed to be aligned. */
typedef struct event_walk_to_parameters {
    u8 unit_id_le[2];
    u8 x;
    u8 y;
    u8 elevation;
    u8 speed_le[2];
    u8 terrain_cost_flag; /* 0 ignores terrain costs; 1 uses them */
} event_walk_to_parameters_t;
typedef char event_walk_to_parameters_size_must_be_8[sizeof(event_walk_to_parameters_t) == 8 ? 1 : -1];

/* Jump operands: unit id, encoded path length, and direction. */
typedef struct event_jump_parameters {
    u8 unit_id_le[2];
    u8 length_plus_one;
    u8 direction;
} event_jump_parameters_t;

typedef struct event_march_parameters {
    u8 unit_id_le[2];
    u8 wait_frames;
} event_march_parameters_t;

/* SpriteMove uses duration_or_speed_le as frames; SpriteMoveBeta as speed. */
typedef struct event_sprite_move_parameters {
    u8 unit_id_le[2];
    u8 destination_le[3][2];
    u8 curve;
    u8 weight;
    u8 duration_or_speed_le[2];
} event_sprite_move_parameters_t;
typedef char event_sprite_move_parameters_size_must_be_12[sizeof(event_sprite_move_parameters_t) == 12 ? 1 : -1];

typedef struct event_unit_anim_parameters {
    u8 unit_id_le[2];
    u8 animation_le[2];
    u8 facing_update_mode;
} event_unit_anim_parameters_t;

/* The animation's high byte also controls facing-update suppression. */
typedef struct event_unit_anim_rotate_parameters {
    u8 unit_id_le[2];
    u8 rotation;
    u8 animation_le[2];
} event_unit_anim_rotate_parameters_t;

typedef struct event_rotate_unit_parameters {
    u8 unit_id_le[2];
    u8 facing;
    u8 mode;
    u8 speed;
    u8 delay;
} event_rotate_unit_parameters_t;

typedef struct event_face_tile_parameters {
    u8 unit_id_le[2];
    u8 x;
    u8 y;
    u8 _unused_04;
    u8 mode;
    u8 speed;
    u8 delay;
} event_face_tile_parameters_t;

typedef struct event_face_unit_parameters {
    u8 first_unit_id_le[2];
    u8 second_unit_id_le[2];
    u8 mode;
    u8 speed;
    u8 delay;
} event_face_unit_parameters_t;

typedef struct event_mirror_sprite_parameters {
    u8 unit_id_le[2];
    u8 mode;
} event_mirror_sprite_parameters_t;

typedef struct event_unit_color {
    u8 mode;
    rgb_s8_t rgb;
    u8 duration;
} event_unit_color_t;

typedef struct event_color_unit_parameters {
    u8 unit_id_le[2];
    event_unit_color_t color;
} event_color_unit_parameters_t;
typedef char event_color_unit_parameters_size_must_be_7[sizeof(event_color_unit_parameters_t) == 7 ? 1 : -1];

typedef struct event_movement_effect_parameters {
    u8 unit_id_le[2];
    u8 suppressed;
} event_movement_effect_parameters_t;

typedef struct event_change_stats_parameters {
    u8 unit_id_le[2];
    u8 stat; /* event_change_stats_e */
    u8 delta_le[2];
} event_change_stats_parameters_t;
typedef char event_change_stats_parameters_size_must_be_5[sizeof(event_change_stats_parameters_t) == 5 ? 1 : -1];

typedef struct event_inflict_status_parameters {
    u8 unit_id_le[2];
    u8 mode; /* event_inflict_status_e */
    u8 wait_frames_le[2];
} event_inflict_status_parameters_t;
typedef char event_inflict_status_parameters_size_must_be_5[sizeof(event_inflict_status_parameters_t) == 5 ? 1 : -1];

/* screen, sound and wait operands */
typedef struct event_background_sound_parameters {
    u8 sound_id;
    u8 volume_from;
    u8 volume_to;
    u8 find_channel;
    u8 frames;
} event_background_sound_parameters_t;
typedef char
    event_background_sound_parameters_size_must_be_5[sizeof(event_background_sound_parameters_t) == 5 ? 1 : -1];

typedef struct event_earthquake_parameters {
    struct {
        u8 z_delta;
        u8 frames;
    } pulses[2];
} event_earthquake_parameters_t;
typedef char event_earthquake_parameters_size_must_be_4[sizeof(event_earthquake_parameters_t) == 4 ? 1 : -1];

typedef struct event_color_screen_parameters {
    u8 mode;
    struct {
        rgb24_t start;
        rgb24_t end;
    } colors;
    u8 duration_le[2];
} event_color_screen_parameters_t;
typedef char event_color_screen_parameters_size_must_be_9[sizeof(event_color_screen_parameters_t) == 9 ? 1 : -1];

/* Six byte-aligned 16-bit lighting targets, followed by the frame duration. */
typedef struct event_map_light_parameters {
    u8 target_rotation_le[3][2];
    u8 target_scale_le[3][2];
    u8 duration_le[2];
} event_map_light_parameters_t;
typedef char event_map_light_parameters_size_must_be_14[sizeof(event_map_light_parameters_t) == 14 ? 1 : -1];

typedef struct event_wait_value_parameters {
    u8 variable_id_le[2];
    u8 test_value_le[2];
} event_wait_value_parameters_t;

/* camera operands */
typedef enum event_camera_component {
    EVENT_CAMERA_COMPONENT_X = 0,
    EVENT_CAMERA_COMPONENT_Z,
    EVENT_CAMERA_COMPONENT_Y,
    EVENT_CAMERA_COMPONENT_TILT,
    EVENT_CAMERA_COMPONENT_YAW,
    EVENT_CAMERA_COMPONENT_ROLL,
    EVENT_CAMERA_COMPONENT_ZOOM
} event_camera_component_e;

enum { EVENT_CAMERA_POSITION_COMPONENT_COUNT = 3, EVENT_CAMERA_COMPONENT_COUNT = 7 };

/* The event byte is widened into a 32-bit setting; bitfield reads change the extraction order. */
enum {
    EVENT_CAMERA_SPEED_CURVE_MODE_MASK = 0x03,
    EVENT_CAMERA_SPEED_CURVE_GROUP_MASK = 0x0c,
    EVENT_CAMERA_SPEED_CURVE_GROUP_SHIFT = 2,
    EVENT_CAMERA_SPEED_CURVE_EASING_MASK = 0xf0,
    EVENT_CAMERA_SPEED_CURVE_EASING_SHIFT = 4,
};

/* Camera operands remain byte-aligned in the event stream. Positions are
 * x, z (height), y; rotations are tilt, yaw, roll. RelativeCamera has the
 * same seven values, and Camera appends a frame duration. */
typedef struct event_camera_values {
    u8 position_le[3][2];
    u8 rotation_le[3][2];
    u8 zoom_le[2];
} event_camera_values_t;
typedef char event_camera_values_size_must_be_14[sizeof(event_camera_values_t) == 14 ? 1 : -1];

typedef struct event_camera_parameters {
    event_camera_values_t values;
    u8 duration_le[2];
} event_camera_parameters_t;
typedef char event_camera_parameters_size_must_be_16[sizeof(event_camera_parameters_t) == 16 ? 1 : -1];

typedef struct event_camera_instruction {
    u8 opcode;
    event_camera_parameters_t parameters;
} event_camera_instruction_t;
typedef char event_camera_instruction_size_must_be_17[sizeof(event_camera_instruction_t) == 17 ? 1 : -1];

/* Focus scans subsequent instructions for the Camera that it rewrites. */
typedef struct event_focus_parameters {
    u8 first_unit_id_le[2];
    u8 second_unit_id_le[2];
    u8 keep_angle;
} event_focus_parameters_t;
typedef char event_focus_parameters_size_must_be_5[sizeof(event_focus_parameters_t) == 5 ? 1 : -1];

/* FocusSpeed rewrites the immediately following Camera's duration. */
typedef struct event_focus_speed_parameters {
    u8 speed_le[2];
} event_focus_speed_parameters_t;

typedef struct event_focus_speed_instruction {
    u8 opcode;
    event_focus_speed_parameters_t parameters;
} event_focus_speed_instruction_t;
typedef char event_focus_speed_instruction_size_must_be_3[sizeof(event_focus_speed_instruction_t) == 3 ? 1 : -1];

#endif
