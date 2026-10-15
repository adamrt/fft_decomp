#ifndef BATTLE_UNIT_H
#define BATTLE_UNIT_H

/* Battle unit records, deployment, animation and display state. */

#include "fft/battle_ai.h"
#include "fft/main.h"

/* sequence and animation types */
/* SEQ sequence bytes, as battle_gfx_run_unit_seq_script (unit sheets) and
 * battle_gfx_run_wep_eff_seq_script (weapon and effect sheets) read them.
 * A sequence is a run of two-byte frame instructions (an SHP frame, then the
 * frames to wait; a wait of 0 holds the frame for good) and of commands:
 * BATTLE_SEQ_OP_PREFIX, an opcode and its operand bytes. A jump offset is a
 * signed byte counted from the command's own prefix byte, so -2 repeats the
 * frame instruction before it. UNKNOWN_XX commands only skip their operands. */
enum {
    BATTLE_SEQ_OP_PREFIX = 0xff,
};

typedef enum battle_seq_opcode {
    /* 0xbe-0xd2 and 0xd4 are unit commands: a weapon or effect sequence that uses one is an error. */
    BATTLE_SEQ_OP_STOP_POSITION_COPIES = 0xbe,
    BATTLE_SEQ_OP_START_POSITION_COPIES = 0xbf, /* the trail of battle_gfx_draw_unit_sprite_position_copies */
    BATTLE_SEQ_OP_WAIT_FOR_DISTORTION = 0xc0,   /* offset: jump while a distortion animation runs */
    BATTLE_SEQ_OP_START_DISTORTION = 0xc1,      /* distortion animation id - 2, timer */
    BATTLE_SEQ_OP_NOP = 0xc2,
    BATTLE_SEQ_OP_HIDE_ITEM = 0xc3,
    BATTLE_SEQ_OP_SET_ITEM_POSITION = 0xc4, /* x, y: signed offsets of the item sprite from the unit */
    BATTLE_SEQ_OP_SHOW_ITEM = 0xc5,         /* draw the unit's item display (battle_gfx_init_item_get_rendering) */
    BATTLE_SEQ_OP_WAIT_FOR_INPUT = 0xc6,    /* offset: jump while g_action_post_action is 0 */
    BATTLE_SEQ_OP_UNKNOWN_C7 = 0xc7,        /* two operands */
    BATTLE_SEQ_OP_UNKNOWN_C8 = 0xc8,        /* two operands */
    BATTLE_SEQ_OP_UNKNOWN_C9 = 0xc9,        /* two operands */
    BATTLE_SEQ_OP_UNKNOWN_CA = 0xca,        /* one operand */
    BATTLE_SEQ_OP_MOVE_UP_2 = 0xcb,         /* 0xcb-0xd2 shift the sprite by one or two units of seq_offset */
    BATTLE_SEQ_OP_MOVE_UP_1 = 0xcc,
    BATTLE_SEQ_OP_MOVE_BACKWARD_2 = 0xcd,
    BATTLE_SEQ_OP_MOVE_BACKWARD_1 = 0xce,
    BATTLE_SEQ_OP_MOVE_DOWN_2 = 0xcf,
    BATTLE_SEQ_OP_MOVE_DOWN_1 = 0xd0,
    BATTLE_SEQ_OP_MOVE_FORWARD_2 = 0xd1,
    BATTLE_SEQ_OP_MOVE_FORWARD_1 = 0xd2,
    BATTLE_SEQ_OP_WAIT_FOR_EFFECT_LOAD = 0xd3, /* offset: jump while g_battle_state_animation_continue_check is 1 */
    BATTLE_SEQ_OP_PLAY_WEAPON_SOUND = 0xd4,    /* 0 swing, 1 hit: sound_play_weapon_sfx */
    BATTLE_SEQ_OP_RESTART = 0xd5,              /* back to the sequence's first byte, counting the restart */
    BATTLE_SEQ_OP_WAIT_FOR_EFFECT = 0xd6,      /* offset: jump while g_battle_state_animation_continue_check is not 0 */
    BATTLE_SEQ_OP_UNKNOWN_D7 = 0xd7,           /* one operand; the sequences of death and revival start with it */
    BATTLE_SEQ_OP_SET_FRAME_BIAS = 0xd8,       /* signed; added to every later frame id */
    BATTLE_SEQ_OP_LAUNCH_PROJECTILE = 0xd9,    /* two unread operands: the thrown item, stone or arrow of the action */
    BATTLE_SEQ_OP_UNDEFINED_DA = 0xda,         /* no handler: MAIN_SYSTEM_ERROR_SEQ_OPCODE like any other value */
    BATTLE_SEQ_OP_SET_WAIT_BIAS = 0xdb,        /* signed; added to every later wait */
    BATTLE_SEQ_OP_RETURN = 0xdc,               /* resume the sequence BATTLE_SEQ_OP_CALL_SEQUENCE left */
    BATTLE_SEQ_OP_CALL_SEQUENCE = 0xdd,        /* sequence id: run it from its start; one level deep */
    BATTLE_SEQ_OP_STRIKE_TARGETS = 0xde,       /* the blow of a weapon-like ability lands: its targets react */
    BATTLE_SEQ_OP_CLEAR_ROTATION = 0xdf,
    BATTLE_SEQ_OP_HIDE_SHADOW = 0xe0,
    BATTLE_SEQ_OP_SHOW_SHADOW = 0xe1,
    BATTLE_SEQ_OP_SET_LAYER_PRIORITY = 0xe2, /* row of g_battle_gfx_animation_layer_priorities */
    BATTLE_SEQ_OP_UNKNOWN_E3 = 0xe3,         /* one operand */
    BATTLE_SEQ_OP_UNKNOWN_E4 = 0xe4,         /* one operand */
    BATTLE_SEQ_OP_SET_ROTATION = 0xe5,       /* low byte, high byte: the display's rotation, ONE to the turn */
    BATTLE_SEQ_OP_UNKNOWN_E6 = 0xe6,         /* two operands */
    BATTLE_SEQ_OP_UNKNOWN_E7 = 0xe7,         /* two operands */
    BATTLE_SEQ_OP_UNKNOWN_E8 = 0xe8,         /* two operands */
    BATTLE_SEQ_OP_UNKNOWN_E9 = 0xe9,         /* one operand */
    BATTLE_SEQ_OP_UNKNOWN_EA = 0xea,         /* two operands */
    BATTLE_SEQ_OP_FLIP_VERTICAL = 0xeb,
    BATTLE_SEQ_OP_FLIP_HORIZONTAL = 0xec,
    BATTLE_SEQ_OP_UNKNOWN_ED = 0xed,           /* one operand */
    BATTLE_SEQ_OP_MOVE_UNIT_FB = 0xee,         /* signed: forward (+) or backward; 0xee-0xf0 and 0xfa move units only */
    BATTLE_SEQ_OP_MOVE_UNIT_DU = 0xef,         /* signed: down (+) or up */
    BATTLE_SEQ_OP_MOVE_UNIT_RL = 0xf0,         /* signed: right (+) or left */
    BATTLE_SEQ_OP_UNKNOWN_F1 = 0xf1,           /* two operands */
    BATTLE_SEQ_OP_START_BLOCK_SEQUENCE = 0xf2, /* sprite block 1-3, sequence id: show the weapon or effect sprite */
    BATTLE_SEQ_OP_UNKNOWN_F3 = 0xf3,           /* one operand */
    BATTLE_SEQ_OP_UNKNOWN_F4 = 0xf4,           /* one operand */
    BATTLE_SEQ_OP_UNKNOWN_F5 = 0xf5,           /* one operand */
    BATTLE_SEQ_OP_PLAY_SOUND = 0xf6,           /* main_sfx_e */
    BATTLE_SEQ_OP_UNKNOWN_F7 = 0xf7,           /* three operands */
    BATTLE_SEQ_OP_UNKNOWN_F8 = 0xf8,           /* two operands */
    BATTLE_SEQ_OP_MOVE_SCREEN_OFFSET = 0xf9,   /* signed x, y: added to the unit's screen_offset */
    BATTLE_SEQ_OP_MOVE_UNIT = 0xfa,            /* right, down, forward: the operands of 0xf0, 0xef and 0xee */
    BATTLE_SEQ_OP_UNKNOWN_FB = 0xfb,           /* two operands */
    BATTLE_SEQ_OP_REPEAT = 0xfc,               /* offset, count: jump count times, then go on */
    BATTLE_SEQ_OP_JUMP = 0xfd,                 /* offset */
    BATTLE_SEQ_OP_END_ANIMATION = 0xfe,        /* stop; a weapon or effect sprite is hidden too */
    BATTLE_SEQ_OP_PAUSE_ANIMATION = 0xff, /* stop; either one puts a unit in its g_battle_gfx_targeted_frame_* pose */
} battle_seq_opcode_e;

/* A facing is an angle with 0x1000 to the turn. Units rest on the four
 * cardinals; the diagonals are the boundaries a free angle is snapped at. */
typedef enum battle_unit_facing {
    BATTLE_FACING_SOUTH = 0x000,
    BATTLE_FACING_SOUTH_WEST = 0x200,
    BATTLE_FACING_WEST = 0x400,
    BATTLE_FACING_NORTH_WEST = 0x600,
    BATTLE_FACING_NORTH = 0x800,
    BATTLE_FACING_NORTH_EAST = 0xa00,
    BATTLE_FACING_EAST = 0xc00,
    BATTLE_FACING_SOUTH_EAST = 0xe00,
} battle_unit_facing_e;

enum {
    BATTLE_FACING_QUARTER_TURN = 0x400,
    BATTLE_FACING_MASK = 0x0fff,
};

/* ability animation types */
typedef struct battle_ability_animation_data {
    u8 charge_animation_set_id; /* row of g_battle_ability_charge_animation_sets */
    u8 attack_animation;        /* battle_unit_animation_e, or BATTLE_ATTACK_ANIMATION_USE_WEAPON or _USE_ITEM */
    u8 text_display; /* low byte of the message text id (0x1800 | this) shown when the ability hits; 0 none */
} battle_ability_animation_data_t;
typedef char battle_ability_animation_data_size_must_be_3[(sizeof(battle_ability_animation_data_t) == 3) ? 1 : -1];

enum {
    BATTLE_ABILITY_ANIMATION_COUNT = 454,
    BATTLE_CHARGE_ANIMATION_SET_COUNT = 20,
    BATTLE_CHARGE_ANIMATION_VARIANT_COUNT = 2,
};

/* Selectors in attack_animation; other values are battle_unit_animation_e IDs. */
enum {
    BATTLE_ATTACK_ANIMATION_USE_WEAPON = 0,
    BATTLE_ATTACK_ANIMATION_USE_ITEM = 1,
};

/* Unit animation ids: the ones the code and the two animation tables of
 * BATTLE.BIN select by number. Animation n is sequences 2n (seen from the
 * front) and 2n + 1 (from behind) of the sheet's SEQ file; the comments
 * describe the sequences of the human sheets. */
typedef enum battle_unit_animation {
    /* Plays sequences 1 to 5 instead: the standing pose for each direction the unit is seen from. */
    BATTLE_UNIT_ANIMATION_STAND = 2, /* stop, petrify, cursed; the default hit reaction */
    BATTLE_UNIT_ANIMATION_IDLE = 3,
    BATTLE_UNIT_ANIMATION_IDLE_HASTE = 4,
    BATTLE_UNIT_ANIMATION_IDLE_SLOW = 5,
    BATTLE_UNIT_ANIMATION_IDLE_FLYING = 6, /* sheets with the flying flag */
    BATTLE_UNIT_ANIMATION_IDLE_FLYING_HASTE = 7,
    BATTLE_UNIT_ANIMATION_IDLE_FLYING_SLOW = 8,
    BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE
    = 9, /* in water two or more deep (move_get_water_depth); also the crystal sprite */
    BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE_HASTE = 10,
    BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE_SLOW = 11,
    BATTLE_UNIT_ANIMATION_WALK = 12, /* a movement step on foot (battle_unit_set_idle_animation_for_movement) */
    BATTLE_UNIT_ANIMATION_WALK_FAST = 13,
    BATTLE_UNIT_ANIMATION_WALK_SLOW = 14,
    BATTLE_UNIT_ANIMATION_FLY = 18,      /* a flying unit's movement step */
    BATTLE_UNIT_ANIMATION_TREASURE = 21, /* the treasure chest sprite */
    BATTLE_UNIT_ANIMATION_KNEEL = 22,    /* one held frame, the picture of SLEEP: event poison, a failed mimic */
    BATTLE_UNIT_ANIMATION_DEFEND = 23,
    BATTLE_UNIT_ANIMATION_DODGE = 24,
    BATTLE_UNIT_ANIMATION_HURT = 25,
    BATTLE_UNIT_ANIMATION_DEAD = 26,
    BATTLE_UNIT_ANIMATION_RAISE_HANDS = 27, /* healed, blade grasp or catch, crystal pickup */
    /* Jumps for joy; in water one deep and for a rider, small hops instead. */
    BATTLE_UNIT_ANIMATION_LEVEL_UP = 28,
    BATTLE_UNIT_ANIMATION_LEVEL_UP_HOP = 29,
    BATTLE_UNIT_ANIMATION_JUMP = 30,
    BATTLE_UNIT_ANIMATION_FALL = 31,
    BATTLE_UNIT_ANIMATION_LAND = 32, /* after a jump or a fall */
    /* Held while charging: the second animation of several charge animation sets. */
    BATTLE_UNIT_ANIMATION_CHARGE = 34,
    BATTLE_UNIT_ANIMATION_CLIMB = 35,
    BATTLE_UNIT_ANIMATION_SLEEP = 36, /* kneeling: also the critical status */
    BATTLE_UNIT_ANIMATION_CONFUSION = 37,
    BATTLE_UNIT_ANIMATION_SING = 40,
    BATTLE_UNIT_ANIMATION_DANCE = 41,
    BATTLE_UNIT_ANIMATION_CAST_START = 42,  /* the first animation of most charge animation sets */
    BATTLE_UNIT_ANIMATION_CAST_CHARGE = 43, /* its loop, held while charging as CHARGE is */
    /* Arms raised until the effect ends: the usual attack animation of the
     * ability table, and every ability of the RUKA, ARUTE and KANZEN sequences. */
    BATTLE_UNIT_ANIMATION_CAST = 44,
    BATTLE_UNIT_ANIMATION_HOP = 48, /* short hops alternate between the two */
    BATTLE_UNIT_ANIMATION_HOP_ALTERNATE = 49,
    BATTLE_UNIT_ANIMATION_RIDING = 50, /* a mounted unit; the rider while its mount moves */
    BATTLE_UNIT_ANIMATION_FOUND_ITEM = 51,
    BATTLE_UNIT_ANIMATION_DEATH = 52,
    BATTLE_UNIT_ANIMATION_REVIVE = 53,
    BATTLE_UNIT_ANIMATION_HOLD_ITEM = 57,
    BATTLE_UNIT_ANIMATION_TELEPORT = 60,
    /* attacks: HIGH at a target at least 12 higher, LOW at one at least 12 lower */
    BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_HIGH = 61,
    BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_LEVEL = 62,
    BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_LOW = 63,
    BATTLE_UNIT_ANIMATION_SWING_HIGH = 64,
    BATTLE_UNIT_ANIMATION_SWING_LEVEL = 65,
    BATTLE_UNIT_ANIMATION_SWING_LOW = 66,
    BATTLE_UNIT_ANIMATION_THROW_ITEM = 76, /* an item used on a target that is not adjacent */
    BATTLE_UNIT_ANIMATION_THRUST_HIGH = 77,
    BATTLE_UNIT_ANIMATION_THRUST_LEVEL = 78,
    BATTLE_UNIT_ANIMATION_THRUST_LOW = 79,
    BATTLE_UNIT_ANIMATION_GUN_HIGH = 80,
    BATTLE_UNIT_ANIMATION_GUN_LEVEL = 81,
    BATTLE_UNIT_ANIMATION_GUN_LOW = 82,
    BATTLE_UNIT_ANIMATION_BOW_HIGH = 83, /* a bow shot takes its row from effect_get_bow_shot_angle */
    BATTLE_UNIT_ANIMATION_BOW_LEVEL = 84,
    BATTLE_UNIT_ANIMATION_BOW_LOW = 85,
    BATTLE_UNIT_ANIMATION_BOOK = 86,
    BATTLE_UNIT_ANIMATION_INSTRUMENT = 87,
    BATTLE_UNIT_ANIMATION_WEAPON_GUARD_HIGH = 88, /* the attacker stands at least 12 higher */
    BATTLE_UNIT_ANIMATION_WEAPON_GUARD_LEVEL = 89,
    BATTLE_UNIT_ANIMATION_WEAPON_GUARD_LOW = 90,
    BATTLE_UNIT_ANIMATION_SWING_BLOCKED_HIGH = 91, /* the attacker's recoil from a blocked attack */
    BATTLE_UNIT_ANIMATION_SWING_BLOCKED_LEVEL = 92,
    BATTLE_UNIT_ANIMATION_SWING_BLOCKED_LOW = 93,
    BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_BLOCKED_HIGH = 94,
    BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_BLOCKED_LEVEL = 95,
    BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_BLOCKED_LOW = 96,
    BATTLE_UNIT_ANIMATION_THRUST_BLOCKED_HIGH = 97,
    BATTLE_UNIT_ANIMATION_THRUST_BLOCKED_LEVEL = 98,
    BATTLE_UNIT_ANIMATION_THRUST_BLOCKED_LOW = 99,
    BATTLE_UNIT_ANIMATION_JUMP_DEPART = 117, /* the jump status is added in event mode; charging a Jump */
    BATTLE_UNIT_ANIMATION_JUMP_LAND = 118,
    BATTLE_UNIT_ANIMATION_CLOTH_HIGH = 119, /* bags and cloths */
    BATTLE_UNIT_ANIMATION_CLOTH_LEVEL = 120,
    BATTLE_UNIT_ANIMATION_CLOTH_LOW = 121,
} battle_unit_animation_e;

/* Animation ids from 500 up are not doubled into a front and a back
 * sequence: 500 + n and 600 + n play sequence n of EVTCHR slot 0 and 1. */
enum {
    BATTLE_SEQUENCE_EVTCHR_SLOT_0 = 500,
    BATTLE_SEQUENCE_EVTCHR_SLOT_1 = 600,
};

/* Masks for the 32-bit view of the high motion byte at runtime-unit +0x83. */
typedef enum battle_motion_flags {
    BATTLE_MOTION_FLAG_SUPPRESS_FACING_UPDATE = 0x01000000,
    BATTLE_MOTION_FLAG_SUPPRESS_SFX_AND_LANDING_EFFECTS = 0x02000000,
    BATTLE_MOTION_FLAG_SUPPRESS_PALETTE_UPDATE = 0x04000000,
    BATTLE_MOTION_FLAG_ALTERNATE_SHORT_HOP = 0x08000000,
    BATTLE_MOTION_FLAG_FLOAT = 0x10000000,
    BATTLE_MOTION_FLAG_FORCE_ANIMATION_UPDATE = 0x20000000, /* set with each stored animation request */
} battle_motion_flags_e;

/* result and sprite types */
typedef struct action_reward_display {
    u8 earned_experience;     /* +0x0 */
    u8 earned_jp;             /* +0x1 */
    u8 level_for_display;     /* +0x2; nonzero selects the "Level UP!" display */
    u8 job_level_for_display; /* +0x3 */
} action_reward_display_t;
typedef char action_reward_display_size_must_be_4[(sizeof(action_reward_display_t) == 4) ? 1 : -1];

/* Packed image IDs in the action-result status list. */
enum {
    ACTION_STATUS_DISPLAY_IMAGE_MASK = 0x7f,
    ACTION_STATUS_DISPLAY_REMOVAL = 0x80,
};

enum { ACTION_STATUS_DISPLAY_CAPACITY = 27 };

/* Destination bytes and motion flags share one 32-bit movement record. */
typedef struct move_destination {
    u8 destination_x;
    u8 destination_y;
    u8 destination_z;
    u32 suppress_facing_update : 1;
    u32 suppress_sfx_and_landing_effects : 1;
    u32 suppress_palette_update : 1;
    u32 alternate_short_hop : 1;
    u32 floating : 1;
    u32 force_animation_update : 1;
    u32 : 2;
} move_destination_t;
typedef char move_destination_size_must_be_4[sizeof(move_destination_t) == 4 ? 1 : -1];

/* One frame of an SHP file: a 2-byte header followed by 4-byte parts, each a
 * rectangle of the sheet and where it is drawn (battle_gfx_load_trap_and_unit_frame_parts). */
typedef struct battle_gfx_source_part {
    s8 x_shift; /* pixels from the sprite's origin to the part's left and top edges */
    s8 y_shift;
    u16 attributes; /* BATTLE_SHP_PART_* masks */
} battle_gfx_source_part_t;

/* Encoded fields of battle_gfx_source_part_t.attributes. */
enum {
    BATTLE_SHP_PART_TILE_MASK = 0x03ff, /* the picture's place in the sheet in 8-pixel tiles: bits 0-4 u, 5-9 v */
    BATTLE_SHP_PART_SIZE_MASK = 0x3c00, /* bits 10-13: index of g_battle_gfx_part_sizes */
    BATTLE_SHP_PART_FLIP_MASK = 0xc000, /* shifted down 13: BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP and _VERTICAL_FLIP */
};

/* The part size that a unit sheet draws as two 48x24 halves, the upper one nearer the camera. */
enum {
    BATTLE_SHP_PART_SIZE_48X48 = 14,
};

typedef struct battle_gfx_source_frame {
    /* Bits 0-2 are the part count minus one, bits 3-7 index the rotation
     * table g_battle_gfx_sprite_y_rotations (0x80094508). */
    u8 part_count_and_rotation;
    u8 flags; /* the BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT and _BLEND_MASK bits of every part */
    battle_gfx_source_part_t parts[1];
} battle_gfx_source_frame_t;

/* WEP/EFF SHP data: the first frame of each weapon type, then the frame
 * pointers from 0x40. */
typedef struct battle_gfx_weapon_shp {
    u16 first_frame[0x20];                    /* 0x000; indexed by weapon type */
    battle_gfx_source_frame_t* frames[0x200]; /* 0x040 */
} battle_gfx_weapon_shp_t;
typedef char battle_gfx_weapon_shp_size_must_be_2112[sizeof(battle_gfx_weapon_shp_t) == 2112 ? 1 : -1];

/* Native WEP/EFF storage pairs the SHP frame table with its SEQ pointers. */
typedef struct battle_gfx_wep_eff_graphics {
    battle_gfx_weapon_shp_t shp; /* 0x000 */
    u8* seq[0x100];              /* 0x840; sequence command streams */
} battle_gfx_wep_eff_graphics_t;
typedef char battle_gfx_wep_eff_graphics_size_must_be_3136[sizeof(battle_gfx_wep_eff_graphics_t) == 3136 ? 1 : -1];

/* One part of a sprite display: a rectangle of the display's texture page,
 * in pixels, and its place relative to the sprite's origin. */
typedef struct battle_gfx_sprite_part_display_data {
    s8 x_shift;
    s8 y_shift;
    u8 width;
    u8 height;
    u8 u;
    u8 v;
    u8 flags; /* battle_sprite_display_flags_e and battle_sprite_display_blend_mode_e */
} battle_gfx_sprite_part_display_data_t;

typedef char
    battle_gfx_sprite_part_display_data_size_must_be_7[(sizeof(battle_gfx_sprite_part_display_data_t) == 7) ? 1 : -1];

/* A sprite display: texture page, CLUT, color and the parts cut from it.
 * Renderer APIs use this common header and variable part tail; the fixed
 * storage records below describe each slot inside a 0x440-byte runtime record. */
typedef struct battle_gfx_sprite_display_data {
    rgb24_t color; /* 0x00 */
    u8 part_count; /* 0x03; parts in use */
    s16 tpage;     /* 0x04; BATTLE_GFX_TPAGE_*, with battle_sprite_display_blend_mode_e */
    u16 clut;      /* 0x06 */
    u16 scale_x;   /* 0x08; ONE is neutral scale */
    u16 scale_y;   /* 0x0a; ONE is neutral scale */
    /* 0x0c; turns the sprite about its origin, ONE to the turn. Each frame
     * sets it from its header; BATTLE_SEQ_OP_SET_ROTATION and _CLEAR_ROTATION
     * then replace it. */
    u16 rotation;
    battle_gfx_sprite_part_display_data_t parts[0]; /* 0x0e; variable tail */
} battle_gfx_sprite_display_data_t;
typedef char battle_gfx_sprite_display_data_size_must_be_14[sizeof(battle_gfx_sprite_display_data_t) == 14 ? 1 : -1];

typedef struct battle_gfx_unit_sprite_display {
    battle_gfx_sprite_display_data_t header;        /* 0x00 */
    battle_gfx_sprite_part_display_data_t parts[8]; /* 0x0e */
} battle_gfx_unit_sprite_display_t;
typedef char battle_gfx_unit_sprite_display_size_must_be_70[sizeof(battle_gfx_unit_sprite_display_t) == 70 ? 1 : -1];

typedef struct battle_gfx_weapon_sprite_display {
    battle_gfx_sprite_display_data_t header;        /* 0x00 */
    battle_gfx_sprite_part_display_data_t parts[3]; /* 0x0e */
    u8 _padding_23;                                 /* aligns the following display */
} battle_gfx_weapon_sprite_display_t;

typedef char
    battle_gfx_weapon_sprite_display_size_must_be_36[sizeof(battle_gfx_weapon_sprite_display_t) == 36 ? 1 : -1];

typedef struct battle_gfx_single_part_sprite_display {
    battle_gfx_sprite_display_data_t header;        /* 0x00 */
    battle_gfx_sprite_part_display_data_t parts[1]; /* 0x0e */
    u8 _padding_15;                                 /* aligns the following display */
} battle_gfx_single_part_sprite_display_t;
typedef char battle_gfx_single_part_sprite_display_size_must_be_22[sizeof(battle_gfx_single_part_sprite_display_t) == 22
        ? 1
        : -1];

/* Weapon/effect sprite animation block, runtime unit data 0x208, 0x238 and
 * 0x268: block 0 plays the weapon sheet's sequences, blocks 1 and 2 the
 * effect sheet's. It shares the first 0x1c bytes of battle_unit_anim_state_t,
 * which has their meanings, but keeps its display pointer at 0x24
 * (battle_gfx_run_wep_eff_seq_script, battle_gfx_draw_unit_sprite_layers,
 * battle_unit_init). Its SEQ table has no header: seq[sequence_id]
 * is the sequence. */
typedef struct battle_unit_sprite_block {
    u16 trigger;                               /* 0x00; nonzero while the sprite is drawn */
    u16 graphic_type;                          /* 0x02; battle_sprite_graphic_type_e */
    u16 sequence_id;                           /* 0x04 */
    u16 script_pos;                            /* 0x06 */
    u16 frame;                                 /* 0x08; includes the weapon type's first frame */
    u16 wait;                                  /* 0x0a */
    u16 restart_count;                         /* 0x0c */
    u16 saved_sequence_id;                     /* 0x0e */
    u16 saved_script_pos;                      /* 0x10 */
    u16 wait_bias;                             /* 0x12 */
    u16 frame_bias;                            /* 0x14 */
    u16 repeat_count;                          /* 0x16 */
    u16 flags;                                 /* 0x18 */
    u8 _padding_1a[2];                         /* 0x1a; aligns shp */
    battle_gfx_weapon_shp_t* shp;              /* 0x1c */
    u8** seq;                                  /* 0x20 */
    battle_gfx_sprite_display_data_t* display; /* 0x24 */
    u8 _unused_28[8];                          /* 0x28 */
} battle_unit_sprite_block_t;
typedef char battle_unit_sprite_block_size_must_be_48[(sizeof(battle_unit_sprite_block_t) == 48) ? 1 : -1];

/* command types */
/* Manual targeting and WAIT_DIRECTION reuse the AI command storage.
 * The 32-bit facing value overlaps the action's item/type/target-ID bytes. Cursor
 * coordinate readers use signed 16-bit values over the action's target fields. */
typedef union battle_unit_command_state {
    ai_command_t ai;
    struct {
        u8 _unused_00[12];
        s32 facing_hint;
        SVECTOR target_panel; /* vx = panel x, vy = map level, vz = panel y */
    } cursor;
} battle_unit_command_state_t;
typedef char battle_unit_command_state_size_must_be_24[sizeof(battle_unit_command_state_t) == 24 ? 1 : -1];

/* encounter and sprite types */
/* g_battle_entd_selection_mode: which ENTD battle_unit_select_and_open_entd opens. */
typedef enum battle_entd_selection {
    BATTLE_ENTD_SELECTION_SCRIPT_VARIABLE = 0, /* EVENT_SCRIPT_VAR_CURRENT_ENTD */
    BATTLE_ENTD_SELECTION_ENTD_250 = 1,
    BATTLE_ENTD_SELECTION_RANDOM_75_TO_79 = 2,
    BATTLE_ENTD_SELECTION_RANDOM_1_TO_59 = 3,
    BATTLE_ENTD_SELECTION_MAP_ID = 4, /* the map's own id when it is not 0 */
} battle_entd_selection_e;

/* graphic_type of a unit's sprite blocks: which sheet the block animates. */
typedef enum battle_sprite_graphic_type {
    BATTLE_SPRITE_GRAPHIC_UNIT = 0,
    BATTLE_SPRITE_GRAPHIC_WEAPON = 1, /* WEP sheets */
    BATTLE_SPRITE_GRAPHIC_EFFECT = 2, /* EFF sheets */
} battle_sprite_graphic_type_e;

/* battle_walk_path_t.pending_mount: the mount standing on the path's destination tile. */
enum {
    BATTLE_PENDING_MOUNT_ID_MASK = 0x7f,
    BATTLE_PENDING_MOUNT_SET = 0x80,
};

/* Unit SHP frame tables filled by battle_gfx_unpack_unit_shp_data (0x800873bc):
 * SHP header bytes 4..5 and 6..7, then two 0xd0-entry frame pointer tables,
 * each entry a battle_gfx_source_frame_t. The second table draws the same
 * frames without the legs, for a unit standing in water or riding
 * (battle_gfx_select_unit_shp_frame); it repeats the first when the file
 * carries a single frame set (header 8). */
typedef struct battle_gfx_unit_shp_frame_tables {
    s32 first_attack_frame;                  /* 0x000; frames from here up are drawn from the sheet's attack image */
    s32 first_sp2_frame;                     /* 0x004; set from the file and never read */
    battle_gfx_source_frame_t* frames[0xd0]; /* 0x008 */
    battle_gfx_source_frame_t* submerged_frames[0xd0]; /* 0x348 */
} battle_gfx_unit_shp_frame_tables_t;

typedef char
    battle_gfx_unit_shp_frame_tables_size_must_be_1672[sizeof(battle_gfx_unit_shp_frame_tables_t) == 1672 ? 1 : -1];

/* Block view of the numeric display fields at runtime unit data 0x2bc..0x2cf,
 * taken as one pointer by the action result display builders
 * (battle_gfx_build_next_action_result_display, 0x800808b8, battle_gfx_animate_post_action_text). */
typedef struct action_result_display {
    u8 active;                                     /* 0x00 */
    u8 _padding_01;                                /* 0x01; aligns selector */
    u16 selector;                                  /* 0x02; battle_numeric_display_selector_e */
    u16 value;                                     /* 0x04 */
    u16 progress;                                  /* 0x06 */
    battle_gfx_sprite_display_data_t* displays[3]; /* 0x08 */
} action_result_display_t;
typedef char action_result_display_size_must_be_20[(sizeof(action_result_display_t) == 20) ? 1 : -1];

/* Common header of an unpacked unit SEQ table. The pointer tail has 0x100
 * entries in a native table and 0x40 in an EVTCHR slot. */
typedef struct battle_gfx_unit_seq_table {
    s32 first_attack_sequence; /* 0x00; sequences from here use the attack image */
    s32 first_sp2_sequence;    /* 0x04; sequences from here use the SP2 file */
    u8* sequences[0];          /* 0x08; variable pointer tail */
} battle_gfx_unit_seq_table_t;
typedef char battle_gfx_unit_seq_table_size_must_be_8[sizeof(battle_gfx_unit_seq_table_t) == 8 ? 1 : -1];

/* Native table storage: five resident tables and the separate boss table.
 * Their sequences live in the shared sequence-data buffer. */
typedef struct battle_gfx_native_unit_seq_table {
    battle_gfx_unit_seq_table_t header; /* 0x000 */
    u8* sequences[0x100];               /* 0x008 */
} battle_gfx_native_unit_seq_table_t;

typedef char
    battle_gfx_native_unit_seq_table_size_must_be_1032[sizeof(battle_gfx_native_unit_seq_table_t) == 1032 ? 1 : -1];

/* Unit SPR file: 16 palettes, two uncompressed 4bpp image blocks, then
 * a variable-length compressed attack-image stream. */
typedef struct battle_gfx_unit_sprite_file {
    clut_t clut;                          /* 0x0000; 16 palettes of 16 colors */
    u8 pixels[256][128];                  /* 0x0200; 256 x 256 pixels, two per byte */
    u8 portrait_and_body_pixels[32][128]; /* 0x8200; 256 x 32 pixels, two per byte */
    u8 compressed_attack_image[0];        /* 0x9200; variable byte tail */
} battle_gfx_unit_sprite_file_t;
typedef char battle_gfx_unit_sprite_file_size_must_be_37376[sizeof(battle_gfx_unit_sprite_file_t) == 37376 ? 1 : -1];

/* Packed unit SEQ file. Values and offsets are little-endian byte
 * sequences, so this view needs no native 16-bit or 32-bit alignment. */
typedef struct battle_gfx_packed_seq_record {
    u8 first_attack_sequence[2];   /* 0x000; unsigned little-endian 16-bit value */
    u8 first_sp2_sequence[2];      /* 0x002; unsigned little-endian 16-bit value */
    u8 sequence_offsets[0x100][4]; /* 0x004; signed little-endian 32-bit values, -1 means offset zero */
    u8 sequence_data_size[2];      /* 0x404; unsigned little-endian 16-bit value */
    u8 sequence_data[0];           /* 0x406; variable byte tail */
} battle_gfx_packed_seq_record_t;
typedef char battle_gfx_packed_seq_record_size_must_be_1030[sizeof(battle_gfx_packed_seq_record_t) == 1030 ? 1 : -1];

/* animation runtime */
/* Unit sprite animation state passed to the sequence and frame renderers.
 * Weapon/effect sprite blocks share its first 0x1c bytes, with distinct tails. */
typedef struct battle_unit_anim_state {
    u16 trigger;      /* 0x00; nonzero while the sprite is drawn */
    u16 graphic_type; /* 0x02; battle_sprite_graphic_type_e */
    /* 0x04; index of the sequence in the SEQ table. A unit's is its
     * battle_unit_animation_e * 2, + 1 when it faces away from the camera;
     * from BATTLE_SEQUENCE_EVTCHR_SLOT_0 up it is a sequence of an EVTCHR slot. */
    u16 sequence_id;
    u16 script_pos;        /* 0x06; offset of the next byte to read in the sequence */
    u16 frame;             /* 0x08; SHP frame on display */
    u16 wait;              /* 0x0a; frames until the sequence is read again; 0 when it has stopped */
    u16 restart_count;     /* 0x0c; BATTLE_SEQ_OP_RESTART commands run since the sequence started */
    u16 saved_sequence_id; /* 0x0e; where BATTLE_SEQ_OP_RETURN resumes */
    u16 saved_script_pos;  /* 0x10 */
    u16 wait_bias;         /* 0x12; BATTLE_SEQ_OP_SET_WAIT_BIAS: added to each wait read */
    u16 frame_bias;        /* 0x14; BATTLE_SEQ_OP_SET_FRAME_BIAS: added to each frame id read */
    u16 repeat_count;      /* 0x16; jumps the BATTLE_SEQ_OP_REPEAT in progress has left; 0 when none is */
    u16 flags;             /* 0x18; the flip bits of battle_sprite_display_flags_e that the sequence toggled */
    u8 _padding_1a[2];     /* 0x1a; aligns shp */
    battle_gfx_unit_shp_frame_tables_t* shp;       /* 0x1c */
    battle_gfx_unit_seq_table_t* seq;              /* 0x20 */
    battle_gfx_unit_shp_frame_tables_t* saved_shp; /* 0x24; the sheet's own tables while a status sheet replaces them */
    battle_gfx_unit_seq_table_t* saved_seq;        /* 0x28 */
    battle_gfx_sprite_display_data_t* display;     /* 0x2c */
} battle_unit_anim_state_t;
typedef char battle_unit_anim_state_size_must_be_48[sizeof(battle_unit_anim_state_t) == 48 ? 1 : -1];

/* movement types */
/* One movement step: a path element, the unit's current step and the step
 * the movement code is working on. */
typedef struct move_step {
    u8 length : 2;    /* extra tiles covered: the jump length */
    u8 slow : 1;      /* start the step at climb speed (move_update_path_step) */
    u8 climb : 1;     /* continue with a climb-speed step */
    u8 mount : 1;     /* the step ends on another unit */
    u8 higher : 1;    /* destination on the higher elevation */
    u8 direction : 2; /* move_step_direction_e */
} move_step_t;
typedef char move_step_size_must_be_1[sizeof(move_step_t) == 1 ? 1 : -1];

/* Packed half-height offsets at 0x8018f86e. A missing presence flag preserves
 * that side's previous value when applying a saved candidate at 0x80177b64. */
typedef struct move_height_offsets {
    u8 destination_present : 1;
    u8 destination_height_offset : 3;
    u8 source_present : 1;
    u8 source_height_offset : 3;
} move_height_offsets_t;
typedef char move_height_offsets_size_must_be_1[(sizeof(move_height_offsets_t) == 1) ? 1 : -1];

/* Tile coordinates stored in teleport paths and relocation reservations. */
typedef struct battle_tile_coordinates {
    u8 x;
    u8 y;
    u8 elevation;
} battle_tile_coordinates_t;
typedef char battle_tile_coordinates_size_must_be_3[(sizeof(battle_tile_coordinates_t) == 3) ? 1 : -1];

/* Complete movement record copied from 0x8018f7f0 into runtime +0x9c.
 * Count 0xfe selects the explicit teleport destination; 0xff is a failed
 * teleport. Ordinary counts select packed steps, each spanning one or more
 * tiles. Byte alignment preserves the original whole-record copies. */
typedef struct battle_walk_path {
    u8 count; /* 0x00; step count or MOVE_PATH_TELEPORT/TELEPORT_FAILED */
    union {
        move_step_t steps[0x7c]; /* 0x01; packed direction, elevation and length */
        battle_tile_coordinates_t teleport;
    } data;
    u8 flags;                             /* 0x7d; move_effective_flags_e */
    move_height_offsets_t height_offsets; /* 0x7e; source/destination offsets */
    u8 pending_mount;                     /* 0x7f; BATTLE_PENDING_MOUNT_SET | battle id */
} battle_walk_path_t;
typedef char battle_walk_path_size_must_be_128[sizeof(battle_walk_path_t) == 128 ? 1 : -1];

/* battle_walk_path_t.count values that are not a step count
 * (move_build_path_to_tile). */
enum {
    MOVE_PATH_TELEPORT = 0xfe, /* the path bytes hold the destination x, y and level */
    MOVE_PATH_TELEPORT_FAILED = 0xff,
};

/* Travel direction of the step in progress, as move_get_direction derives it from the unit's
 * tile and its destination tile. */
typedef enum move_direction {
    MOVE_DIRECTION_SOUTH = 0, /* -y */
    MOVE_DIRECTION_EAST = 1,  /* +x */
    MOVE_DIRECTION_NORTH = 2, /* +y */
    MOVE_DIRECTION_WEST = 3,  /* -x */
} move_direction_e;

/* move_step_t.direction. */
typedef enum move_step_direction {
    MOVE_STEP_DIRECTION_EAST = 0,  /* +x */
    MOVE_STEP_DIRECTION_WEST = 1,  /* -x */
    MOVE_STEP_DIRECTION_SOUTH = 2, /* -y */
    MOVE_STEP_DIRECTION_NORTH = 3, /* +y */
} move_step_direction_e;

/* battle_unit_t.step_phase: the stage of the movement step in progress. Every stage has one
 * value per move_direction_e direction, in that order. The handlers dispatch on the stage and
 * take the direction from the unit's coordinates, and the stores that change stage in the middle of
 * a step write the NORTH value whatever the direction. */
typedef enum move_step_phase {
    MOVE_STEP_PHASE_NONE = 0,
    /* walking: from the tile center to its exit edge, then on to the destination center */
    MOVE_STEP_PHASE_WALK_TO_EDGE_SOUTH = 1,
    MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH = 2,
    MOVE_STEP_PHASE_WALK_TO_EDGE_EAST = 3,
    MOVE_STEP_PHASE_WALK_TO_CENTER_EAST = 4,
    MOVE_STEP_PHASE_WALK_TO_EDGE_NORTH = 5,
    MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH = 6,
    MOVE_STEP_PHASE_WALK_TO_EDGE_WEST = 7,
    MOVE_STEP_PHASE_WALK_TO_CENTER_WEST = 8,
    /* the same two stages at climb speed */
    MOVE_STEP_PHASE_CLIMB_TO_EDGE_SOUTH = 9,
    MOVE_STEP_PHASE_CLIMB_TO_CENTER_SOUTH = 10,
    MOVE_STEP_PHASE_CLIMB_TO_EDGE_EAST = 11,
    MOVE_STEP_PHASE_CLIMB_TO_CENTER_EAST = 12,
    MOVE_STEP_PHASE_CLIMB_TO_EDGE_NORTH = 13,
    MOVE_STEP_PHASE_CLIMB_TO_CENTER_NORTH = 14,
    MOVE_STEP_PHASE_CLIMB_TO_EDGE_WEST = 15,
    MOVE_STEP_PHASE_CLIMB_TO_CENTER_WEST = 16,
    /* jumping: the take-off animation, rising, falling to the destination edge, the landing animation */
    MOVE_STEP_PHASE_JUMP_START_SOUTH = 17,
    MOVE_STEP_PHASE_JUMP_ASCENT_SOUTH = 18,
    MOVE_STEP_PHASE_JUMP_DESCENT_SOUTH = 19,
    MOVE_STEP_PHASE_JUMP_LANDING_SOUTH = 20,
    MOVE_STEP_PHASE_JUMP_START_EAST = 21,
    MOVE_STEP_PHASE_JUMP_ASCENT_EAST = 22,
    MOVE_STEP_PHASE_JUMP_DESCENT_EAST = 23,
    MOVE_STEP_PHASE_JUMP_LANDING_EAST = 24,
    MOVE_STEP_PHASE_JUMP_START_NORTH = 25,
    MOVE_STEP_PHASE_JUMP_ASCENT_NORTH = 26,
    MOVE_STEP_PHASE_JUMP_DESCENT_NORTH = 27,
    MOVE_STEP_PHASE_JUMP_LANDING_NORTH = 28,
    MOVE_STEP_PHASE_JUMP_START_WEST = 29,
    MOVE_STEP_PHASE_JUMP_ASCENT_WEST = 30,
    MOVE_STEP_PHASE_JUMP_DESCENT_WEST = 31,
    MOVE_STEP_PHASE_JUMP_LANDING_WEST = 32,
    /* flying */
    MOVE_STEP_PHASE_FLY_TO_EDGE_SOUTH = 34,
    MOVE_STEP_PHASE_FLY_TO_CENTER_SOUTH = 35,
    MOVE_STEP_PHASE_FLY_TO_EDGE_EAST = 36,
    MOVE_STEP_PHASE_FLY_TO_CENTER_EAST = 37,
    MOVE_STEP_PHASE_FLY_TO_EDGE_NORTH = 38,
    MOVE_STEP_PHASE_FLY_TO_CENTER_NORTH = 39,
    MOVE_STEP_PHASE_FLY_TO_EDGE_WEST = 40,
    MOVE_STEP_PHASE_FLY_TO_CENTER_WEST = 41,
    /* floating: the two walking stages, and the jump up to a higher tile */
    MOVE_STEP_PHASE_FLOAT_TO_EDGE_SOUTH = 43,
    MOVE_STEP_PHASE_FLOAT_TO_CENTER_SOUTH = 44,
    MOVE_STEP_PHASE_FLOAT_JUMP_SOUTH = 45,
    MOVE_STEP_PHASE_FLOAT_TO_EDGE_EAST = 47,
    MOVE_STEP_PHASE_FLOAT_TO_CENTER_EAST = 48,
    MOVE_STEP_PHASE_FLOAT_JUMP_EAST = 49,
    MOVE_STEP_PHASE_FLOAT_TO_EDGE_NORTH = 51,
    MOVE_STEP_PHASE_FLOAT_TO_CENTER_NORTH = 52,
    MOVE_STEP_PHASE_FLOAT_JUMP_NORTH = 53,
    MOVE_STEP_PHASE_FLOAT_TO_EDGE_WEST = 55,
    MOVE_STEP_PHASE_FLOAT_TO_CENTER_WEST = 56,
    MOVE_STEP_PHASE_FLOAT_JUMP_WEST = 57,
    /* a teleport path (MOVE_PATH_TELEPORT and _FAILED): start the animation, then wait for it */
    MOVE_STEP_PHASE_TELEPORT_START = 59,
    MOVE_STEP_PHASE_TELEPORT_ANIMATION = 60,
} move_step_phase_e;

/* distortion and strike types */
/* battle_unit_t.distortion_animation_id: the motion that
 * battle_unit_dispatch_distortion_animation steps once an animation frame
 * until its handler clears the id. BATTLE_SEQ_OP_START_DISTORTION gives the
 * id less 2 and, for most, the frames it takes (distortion_timer). */
typedef enum battle_distortion_animation {
    BATTLE_DISTORTION_ANIMATION_NONE = 0,
    BATTLE_DISTORTION_ANIMATION_SCALE_PULSE = 1,      /* squeezes the sprite thin and tall; nothing starts it */
    BATTLE_DISTORTION_ANIMATION_RISE = 2,             /* 0x24 up */
    BATTLE_DISTORTION_ANIMATION_SETTLE = 3,           /* back to the height of where the unit stands */
    BATTLE_DISTORTION_ANIMATION_MOVE_TO_TARGET = 4,   /* to the first target, or to the targeted panel */
    BATTLE_DISTORTION_ANIMATION_MOVE_TO_OWN_TILE = 5, /* to the center of the unit's own tile */
    BATTLE_DISTORTION_ANIMATION_JUMP_TO_TARGET = 6,   /* the same two along a jump arc */
    BATTLE_DISTORTION_ANIMATION_JUMP_TO_OWN_TILE = 7,
    BATTLE_DISTORTION_ANIMATION_MOVE_TO_TARGET_LEVEL = 8, /* 4 and 5 without the change of height */
    BATTLE_DISTORTION_ANIMATION_MOVE_TO_OWN_TILE_LEVEL = 9,
    BATTLE_DISTORTION_ANIMATION_CLEAR = 10, /* 10 and 11 clear the id at once */
    BATTLE_DISTORTION_ANIMATION_CLEAR_2 = 11,
    BATTLE_DISTORTION_ANIMATION_JUMP_START = 12,     /* straight up to 0x120 above the ground, trailing copies */
    BATTLE_DISTORTION_ANIMATION_FALL_TO_TARGET = 13, /* down onto the target's tile */
    BATTLE_DISTORTION_ANIMATION_TELEPORT = 14,       /* fade out, move to the step's destination, fade in */
    BATTLE_DISTORTION_ANIMATION_BACK_AWAY = 15,      /* part of a tile away from the target */
    BATTLE_DISTORTION_ANIMATION_JUMP_TO_OWN_TILE_FADED = 16, /* 7, blended, and dark until four frames remain */
    BATTLE_DISTORTION_ANIMATION_JUMP_RISE = 17, /* 12 with the jump sound, unless the unit is that high already */
} battle_distortion_animation_e;

enum {
    BATTLE_DISTORTION_ANIMATION_COUNT = 18, /* the handlers of g_battle_unit_distortion_animation_handlers */
};

/* The strike work record is embedded in unit runtime record at +0x18c
 * and used as a standalone AI work record. Reaction targeting writes its
 * redirected tile at +0x1c..+0x1e before the strike is processed. */
typedef struct battle_strike_work {
    u8 actor_id;
    u8 target_count;
    u8 target_list[BATTLE_UNIT_RUNTIME_SLOT_COUNT];
    u8 skip_empty_tile_effect;
    u8 math_skill_flag;
    s16 last_attack_id; /* 0x14 */
    u8 post_action_message;
    u8 reaction_occurred;
    u8 continue_attack;
    u8 current_hit_number;
    s16 reaction_id;
    u8 target_new_x;
    u8 target_new_y;
    u8 target_new_map_level;
    u8 used_weapon_id;
    s16 reaction_ability_id; /* 0x20; runtime 0x1ac */
    /* 0x22; runtime 0x1ae. g_action_current_ability.knockback_flags & 0x7f, stored with
     * the knockback destination. */
    u8 knockback_flags;
    u8 can_earn_experience;
    action_reward_display_t action_rewards; /* 0x24; runtime 0x1b0..0x1b3 */
} battle_strike_work_t;
typedef char battle_strike_work_must_be_40[sizeof(battle_strike_work_t) == 40 ? 1 : -1];

/* unit runtime */
/* Live on-map units own movement, animation, commands and displays.
 * runtime_id indexes g_battle_units; stats optionally links combat stats. */
typedef struct battle_unit {
    struct battle_unit* previous; /* 0x000; previous unit in the runtime list */
    u8 runtime_id;                /* 0x004; runtime slot ID */
    u8 spritesheet_vram_slot;     /* 0x005 */
    u8 spritesheet_id;            /* 0x006 */
    u8 stored_palette;            /* 0x007; the palette of its sheet that the unit wears, 0 to 5 */
    u16 state_frame_counter;      /* 0x008; incremented while renderer action states wait */
    u16 draw_enabled;             /* 0x00a; set by event Draw, cleared by Erase; a 16-bit value */
    u16 requested_animation;      /* 0x00c; battle_unit_animation_e + 1 to start at the next update; 0 none */
    u16 tpage;                    /* 0x00e; VRAM slot + 0x14: the sheet's texture page */
    u16 clut;                     /* 0x010 */
    /* battle_sprite_display_flags_e and battle_sprite_display_blend_mode_e.
     * Read-modify-written as a 16-bit value over
     * 0x012..0x013 (battle_gfx_update_sprite_transparency_flag). */
    u16 sprite_display_flags; /* 0x012 */
    /* 0x014; BATTLE_SEQ_OP_SET_LAYER_PRIORITY: the row of g_battle_gfx_animation_layer_priorities. */
    u16 layer_priority;
    u8 _padding_016[2]; /* aligns real */
    VECTOR real;        /* 0x018; world coordinates in 20.12 */
    /* Per-frame real-coordinate velocity; the jump-start distortion animation
     * (0x8008a35c) copies all four 32-bit values as one block. */
    VECTOR velocity; /* 0x028 */
    /* Movement interpolation speed, stored and passed as a signed 32-bit value. */
    s32 walk_speed;       /* 0x038 */
    s32 step_speed;       /* 0x03c; interpolation speed of the current step; 0x2000 when a move/climb starts, raised by
                             jump gravity */
    SVECTOR world_coords; /* 0x040; integer world coordinates: x, height, map depth */
    u8 _unused_048[8];
    /* The sprite is drawn at world_coords + event_offset + seq_offset
     * (battle_unit_copy_draw_coords); both offsets are in the units
     * of world_coords (vx = x, vy = height, vz = map depth). */
    SVECTOR seq_offset; /* 0x050; the shift of the running sequence's move commands; cleared when one starts */
    /* Added to the projected sprite position, in pixels, by
     * BATTLE_SEQ_OP_MOVE_SCREEN_OFFSET; cleared when a sequence starts and,
     * with the other two offsets, by battle_unit_place_in_new_location. vz
     * is only ever cleared. */
    SVECTOR screen_offset; /* 0x058 */
    SVECTOR event_offset;  /* 0x060; accumulated by the event SpriteMove instructions */
    u8 _unused_068[4];
    /* Camera-relative facing, ((camera yaw + facing) & 0xfff) / 1024 and / 256.
     * Initialized to -1 and copied from mount to rider. */
    s16 camera_facing_quadrant;   /* 0x06c */
    s16 camera_facing_sixteenth;  /* 0x06e */
    s16 facing;                   /* 0x070 */
    s16 attack_facing;            /* 0x072 */
    s16 depth_height_offset;      /* 0x074 */
    u16 float_bob_offset;         /* 0x076; from the float bob table */
    u16 float_bob_phase;          /* 0x078; advanced by g_main_system_animation_speed at 0x8007ea98 */
    u16 special_graphic_y_offset; /* 0x07a; added to the v of each part: a status sheet's rows in OTHER.SPR */
    u8 map_x;                     /* 0x07c */
    u8 map_y;                     /* 0x07d */
    u8 map_z;                     /* 0x07e */
    u8 step_phase;                /* 0x07f; move_step_phase_e */
    move_destination_t movement;
    u8 previous_map_x;                 /* 0x084 */
    u8 previous_map_y;                 /* 0x085 */
    u8 previous_map_z;                 /* 0x086 */
    u8 distortion_animation_id;        /* 0x087; battle_distortion_animation_e */
    s32 distortion_phase;              /* 0x088; the handler's own step, 0 when the distortion animation starts */
    s32 distortion_timer;              /* 0x08c; frames left: the second operand of BATTLE_SEQ_OP_START_DISTORTION */
    s32 distortion_target;             /* 0x090; target real.vy or frame threshold of the distortion step */
    u16 previous_facing;               /* 0x094 */
    u8 destination_edge_height;        /* 0x096; destination slope corner, as g_move_destination_edge_height */
    u8 current_edge_height;            /* 0x097; current slope corner, as g_move_current_edge_height */
    u32 movement_path_offset;          /* 0x098; index of the next packed path step */
    battle_walk_path_t path;           /* 0x09c..0x11b */
    move_step_t current_path_step;     /* 0x11c; path step byte being executed */
    u8 last_path_count;                /* 0x11d; receives prior movement-path count at 0x8006d7b8 */
    u8 standing_on_battle_id_plus_one; /* 0x11e; unit stood on, 0 = none */
    u8 previous_standing_on_battle_id_plus_one; /* 0x11f; restored when a move is undone */
    s16 camera_relative_x;                      /* 0x120; camera-space x from RotTrans */
    s16 camera_relative_y;                      /* 0x122 */
    u8 _unused_124[4];
    u32 otag_depth_index;                 /* 0x128 */
    s32 gte_flag;                         /* 0x12c; RotTrans flag output of the unit and status bubble transforms */
    u8 mount_state;                       /* 0x130; battle_unit_mount_state_e */
    u8 mount_partner_runtime_id;          /* 0x131; linked rider or mount runtime ID */
    u8 previous_mount_state;              /* 0x132; battle_unit_mount_state_e */
    u8 previous_mount_partner_runtime_id; /* 0x133 */
    unit_stats_t* stats;                  /* 0x134; unit_stats_t backlink */
    u16 used_ability_id;                  /* 0x138 */
    u8 used_item_or_weapon_id;            /* 0x13a; the item the weapon and effect blocks draw */
    u8 equipped_weapon_type;              /* 0x13b; its item type, or 0x15 thrown: the frame set of the weapon SHP */
    u8 turn_action_flags;                 /* 0x13c; |= 1 moved, |= 2 acted this turn */
    u8 team_flags;                        /* 0x13d */
    u8 fade_out_phase;                    /* 0x13e; 0 none, 1 flash, 2 darken, 3 removed */
    u8 horizontal_flip_flag; /* 0x13f; BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP or 0: the event MirrorSprite */
    u32 status_flags_1_4;    /* 0x140; battle_unit_status_flags_1_4_e */
    u32 status_flags_5_6;    /* 0x144; battle_unit_status_flags_5_6_e */
    /* 0x148..0x157: statuses to add/remove;
     * battle_unit_update_float_and_jump_height tests the pending removal of Jump. */
    u32 statuses_to_add_1_4;                   /* 0x148 */
    u32 statuses_to_add_5_6;                   /* 0x14c */
    u32 statuses_to_remove_1_4;                /* 0x150 */
    u32 statuses_to_remove_5_6;                /* 0x154 */
    battle_unit_command_state_t command_state; /* 0x158..0x16f */
    s32 sp2_ability_id;                        /* 0x170: ability whose SP2 file battle_open_sp2 loads */
    s32 ability_preview_phase;                 /* 0x174: target_panels_e */
    /* 0x178; target_calculate_for_menu_types result stored by
     * target_select_tile: 0/1 preview, 2 stop, -1 back to the menu. */
    s32 target_select_result;
    s32 attack_phase_state;    /* 0x17c: battle_attack_preparation_e */
    s32 pending_attack_result; /* 0x180: attack result code; -1 signals death */
    /* Death-by-dismount destination (unit runtime record 0x184/0x186/
     * 0x188); battle_unit_set_map_coords_after_death_dismount copies them to
     * map_x/map_z/map_y.  attack_result_animation_update passes &dismount. */
    map_tile_coordinates_t dismount; /* 0x184 */
    u8 _unused_18a[2];
    battle_strike_work_t strike; /* 0x18c..0x1b3; current action targets, result and rewards */
    u8 command_ready;            /* 0x1b4; 0x800753b8 sets 1 after AI setup completes; gates command dispatch */
    /* 0x1b5; strikes run after the first while continue_attack is set; zeroed
     * for each new action. */
    u8 continue_attack_count;
    /* 0x1b6; the auto-battle command kind stored before target select (the
     * TARGETED_SELECT kind requires an enemy, RECOVERY_SELECT an ally) or target
     * display (the two DISPLAY kinds). */
    u8 target_select_command;
    u8 status_display_image_count; /* 0x1b7; capped at 0x1b */
    u32 action_display_flags;      /* 0x1b8..0x1bb */
    /* Action-display image IDs, not canonical status IDs.  Bit 0x80 marks a
     * removal; the dispatcher consumes entries from the end. */
    u8 status_display_image_list[ACTION_STATUS_DISPLAY_CAPACITY]; /* 0x1bc..0x1d6 */
    u8 _padding_1d7;                                              /* aligns animation */
    battle_unit_anim_state_t animation;                           /* 0x1d8..0x207 */
    battle_unit_sprite_block_t sprite_blocks[3];                  /* 0x208..0x297 */
    u8 shadow_graphic_trigger;                                    /* 0x298; nonzero: draw the unit's shadow */
    u8 shadow_dirty;        /* 0x299; bit 0 set when a step lands, cleared on shadow redraw */
    u8 shadow_counter;      /* 0x29a; shadow animation counter (battle_gfx_draw_unit_shadow) */
    u8 _padding_29b;        /* aligns shadow_quad */
    SVECTOR shadow_quad[4]; /* 0x29c; shadow corners written by battle_gfx_calculate_sprite_shadow_from_tile_slope */
    action_result_display_t numeric_display; /* 0x2bc..0x2cf */
    u8 item_display_active;                  /* 0x2d0; BATTLE_SEQ_OP_SHOW_ITEM: draw item_display */
    u8 _unused_2d1;                          /* 0x2d1 */
    s8 item_get_x_offset;                    /* 0x2d2; BATTLE_SEQ_OP_SET_ITEM_POSITION, in pixels */
    s8 item_get_y_offset;                    /* 0x2d3 */
    u8 _unused_2d4[4];
    battle_gfx_sprite_display_data_t* item_display; /* 0x2d8; item/Equip display pointer */
    /* Status bubble (0x2dc..0x2e7);
     * battle_gfx_update_status_bubble_graphic_trigger sets the flag and clears
     * the timer with a 16-bit store, so the timer is declared u16. */
    u8 status_bubble_active;                                 /* 0x2dc */
    u8 status_bubble_id;                                     /* 0x2dd */
    s8 status_bubble_x;                                      /* 0x2de */
    s8 status_bubble_y;                                      /* 0x2df */
    u16 status_bubble_timer;                                 /* 0x2e0 */
    u16 status_bubble_alternate_row;                         /* 0x2e2; non-zero selects the second texture row */
    battle_gfx_sprite_display_data_t* status_bubble_display; /* 0x2e4 */
    /* 0x2e8; enables the six-copy position trail seeded by
     * battle_gfx_init_position_vector_copies. */
    u32 position_copies_active;
    /* Six vectors used during sprite rotation/scaling render (0x2ec);
     * battle_gfx_init_position_vector_copies seeds all six from world_coords. */
    SVECTOR position_copies[6]; /* 0x2ec..0x31b */
    /* Fixed sprite-display storage; API pointers address each header. */
    battle_gfx_unit_sprite_display_t unit_sprite_display;                 /* 0x31c; 8 parts */
    battle_gfx_weapon_sprite_display_t weapon_sprite_displays[3];         /* 0x362; 3 parts each */
    battle_gfx_single_part_sprite_display_t numeric_sprite_displays[3];   /* 0x3ce */
    battle_gfx_single_part_sprite_display_t status_bubble_sprite_display; /* 0x410 */
    battle_gfx_single_part_sprite_display_t item_sprite_display;          /* 0x426 */
    u8 _unused_43c[4]; /* 0x43c; the next slot's in-use 32-bit value (g_battle_unit_slot_states) */
} battle_unit_t;
typedef char battle_unit_size_must_be_1088[(sizeof(battle_unit_t) == 1088) ? 1 : -1];

/* battle_unit_t.fade_out_phase: the fade-out removal's progress. */
typedef enum battle_unit_fade_out_phase {
    BATTLE_UNIT_FADE_OUT_NONE = 0,
    BATTLE_UNIT_FADE_OUT_FLASH = 1,
    BATTLE_UNIT_FADE_OUT_DARKEN = 2,
    BATTLE_UNIT_FADE_OUT_REMOVED = 3,
} battle_unit_fade_out_phase_e;

/* battle_unit_t.turn_action_flags. */
typedef enum battle_unit_turn_action_flags {
    BATTLE_UNIT_TURN_ACTION_MOVED = 0x01,
    BATTLE_UNIT_TURN_ACTION_ACTED = 0x02,
} battle_unit_turn_action_flags_e;

/* unit flags */
/* unit runtime record 0x130 identifies which role 0x131 links. */
typedef enum battle_unit_mount_state {
    BATTLE_UNIT_MOUNT_STATE_NONE = 0,
    BATTLE_UNIT_MOUNT_STATE_RIDER = 1,
    BATTLE_UNIT_MOUNT_STATE_MOUNT = 2,
} battle_unit_mount_state_e;

/* Result of battle_unit_get_chocobo_mount_state. */
typedef enum battle_chocobo_mount_state {
    BATTLE_CHOCOBO_MOUNT_NONE = 0, /* not a chocobo, or one in an unmountable status */
    BATTLE_CHOCOBO_MOUNT_FREE = 1,
    BATTLE_CHOCOBO_MOUNT_PAIRED = 2, /* already carries a rider */
} battle_chocobo_mount_state_e;

/* battle_unit_learn_from_crystal flags. */
enum {
    BATTLE_CRYSTAL_LEARN_ABILITIES = 0x1,
    BATTLE_CRYSTAL_RESTORE_HP_MP = 0x2,
    BATTLE_CRYSTAL_TAKE_TREASURE = 0x4, /* add the treasure's item instead */
};

/* Results of battle_unit_find_slot_by_character_identity that are not a stats slot. */
typedef enum battle_character_slot {
    BATTLE_CHARACTER_SLOT_SELECTOR = -1, /* the identity is a selector value, not a character */
    BATTLE_CHARACTER_SLOT_OFF_FIELD = -2,
    BATTLE_CHARACTER_SLOT_UNAVAILABLE = -3, /* a crystal or a treasure, or a disabled unit */
    BATTLE_CHARACTER_SLOT_IN_PARTY = -4,    /* only in the party roster */
    BATTLE_CHARACTER_SLOT_ABSENT = -5,
} battle_character_slot_e;

/* Result of battle_unit_get_action_restriction. */
typedef enum battle_unit_action_restriction {
    BATTLE_UNIT_ACTION_RESTRICTION_NONE = 0,
    /* Don't Act status or BATTLE_MOUNT_INFO_FLAG_MOUNT. */
    BATTLE_UNIT_ACTION_RESTRICTION_CANNOT_ACT = 1,
    /* Tile depth 2 or more, unless the unit has Float, Chicken or Frog status,
     * a water movement ability or BATTLE_MOUNT_INFO_FLAG_RIDER. */
    BATTLE_UNIT_ACTION_RESTRICTION_DEEP_WATER = 2,
} battle_unit_action_restriction_e;

/* How battle_gfx_construct_polygon_data_for_units draws a sprite part. The
 * unit's sprite_display_flags, its horizontal_flip_flag, the flags a
 * sequence toggles and each part's own are XORed together. */
typedef enum battle_sprite_display_flags {
    BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT = 0x01,
    BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP = 0x02,
    BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP = 0x04,
    /* A part's flags only: queued nearer the camera than the rest by its pitch and zoom. */
    BATTLE_SPRITE_DISPLAY_NEARER = 0x80,
} battle_sprite_display_flags_e;

/* Two-bit selector in sprite flags and tpage, already shifted to bits 5-6. */
typedef enum battle_sprite_display_blend_mode {
    BATTLE_SPRITE_DISPLAY_BLEND_AVERAGE = 0x00,
    BATTLE_SPRITE_DISPLAY_BLEND_ADD = 0x20,
    BATTLE_SPRITE_DISPLAY_BLEND_SUBTRACT = 0x40,
    BATTLE_SPRITE_DISPLAY_BLEND_ADD_QUARTER = 0x60,
} battle_sprite_display_blend_mode_e;

enum { BATTLE_SPRITE_DISPLAY_BLEND_MASK = 0x60 };

typedef enum battle_unit_height_flags {
    BATTLE_UNIT_HEIGHT_NON_CHOCOBO = 0x20,
    BATTLE_UNIT_HEIGHT_SHARED_TILE = 0x40,
    BATTLE_UNIT_HEIGHT_UNAVAILABLE = 0x80,
} battle_unit_height_flags_e;

/* Selection policy for battle_unit_find_at_tile.  Status bits are inclusive
 * alternatives; the Chocobo bit is an additional requirement. */
typedef enum battle_unit_tile_filter {
    BATTLE_UNIT_TILE_FILTER_TREASURE = 0x01,
    BATTLE_UNIT_TILE_FILTER_DRY_LOW_SLOPE = 0x02,
    BATTLE_UNIT_TILE_FILTER_TRANSPARENT = 0x10,
    BATTLE_UNIT_TILE_FILTER_CRYSTAL = 0x40,
    BATTLE_UNIT_TILE_FILTER_MOUNTABLE_CHOCOBO = 0x80,
    BATTLE_UNIT_TILE_STATUS_FILTERS
    = BATTLE_UNIT_TILE_FILTER_TREASURE | BATTLE_UNIT_TILE_FILTER_TRANSPARENT | BATTLE_UNIT_TILE_FILTER_CRYSTAL,
} battle_unit_tile_filter_e;

/* unit sequences and statuses */
/* An EVTCHR slot's fixed SEQ storage. Its 32-bit header fields are unset and unread;
 * its 0x40 pointers refer into the slot's own 0x400 bytes. */
typedef struct battle_gfx_unit_seq {
    battle_gfx_unit_seq_table_t header; /* 0x000 */
    u8* sequences[0x40];                /* 0x008 */
    u8 sequence_data[0x400];            /* 0x108 */
} battle_gfx_unit_seq_t;
typedef char battle_gfx_unit_seq_size_must_be_1288[sizeof(battle_gfx_unit_seq_t) == 1288 ? 1 : -1];

/* Renderer-side runtime unit status mirror. These packed masks do not use the
 * canonical status_id_e byte order. */
typedef enum battle_unit_status_flags_1_4 {
    /* runtime +0x140. Bit 0x00000001 is tested for animation 0x21 and never set. */
    BATTLE_UNIT_STATUS_CONFUSION = 0x00000080,
    BATTLE_UNIT_STATUS_SLEEP = 0x00000040,
    BATTLE_UNIT_STATUS_PETRIFY = 0x00000020,
    BATTLE_UNIT_STATUS_STOP = 0x00000010,
    BATTLE_UNIT_STATUS_CRITICAL = 0x00000008,
    BATTLE_UNIT_STATUS_DEAD = 0x00000004,
    BATTLE_UNIT_STATUS_MOUNTED = 0x00000002,

    /* runtime +0x141. */
    BATTLE_UNIT_STATUS_FAITH = 0x00008000,
    BATTLE_UNIT_STATUS_BERSERK = 0x00004000,
    BATTLE_UNIT_STATUS_UNNAMED_0E = 0x00002000,
    BATTLE_UNIT_STATUS_HASTE = 0x00001000,
    BATTLE_UNIT_STATUS_SLOW = 0x00000800,
    BATTLE_UNIT_STATUS_DEFENDING = 0x00000400,
    BATTLE_UNIT_STATUS_CHARGING = 0x00000200,
    BATTLE_UNIT_STATUS_PERFORMING = 0x00000100,

    /* runtime +0x142. */
    BATTLE_UNIT_STATUS_OIL = 0x00800000,
    BATTLE_UNIT_STATUS_POISON = 0x00400000,
    BATTLE_UNIT_STATUS_PROTECT = 0x00200000,
    BATTLE_UNIT_STATUS_SHELL = 0x00100000,
    BATTLE_UNIT_STATUS_DONT_ACT = 0x00080000,
    BATTLE_UNIT_STATUS_DONT_MOVE = 0x00040000,
    BATTLE_UNIT_STATUS_BLOOD_SUCK = 0x00020000,
    BATTLE_UNIT_STATUS_INNOCENT = 0x00010000,

    /* runtime +0x143. Bit 0x80000000 remains unknown. */
    BATTLE_UNIT_STATUS_UNDEAD = 0x40000000,
    BATTLE_UNIT_STATUS_REGEN = 0x20000000,
    BATTLE_UNIT_STATUS_RERAISE = 0x10000000,
    BATTLE_UNIT_STATUS_DARKNESS = 0x08000000,
    BATTLE_UNIT_STATUS_DEATH_SENTENCE = 0x04000000,
    BATTLE_UNIT_STATUS_SILENCE = 0x02000000,
    BATTLE_UNIT_STATUS_CHARM = 0x01000000,

    BATTLE_UNIT_STATUS_PALETTE_MOD_MASK = 0x60c26020,
    BATTLE_UNIT_STATUS_BUBBLE_MASK = 0x1fffc2c4,
    BATTLE_UNIT_STATUS_PREVENT_FACING_MASK = 0x00080074,
    BATTLE_UNIT_STATUS_ANIMATION_SELECTION_MASK = 0x00003fff,
} battle_unit_status_flags_1_4_e;

typedef enum battle_unit_status_flags_5_6 {
    /* runtime +0x144. */
    BATTLE_UNIT_STATUS_CRYSTAL = 0x00000001,
    BATTLE_UNIT_STATUS_CHICKEN = 0x00000002,
    BATTLE_UNIT_STATUS_FROG = 0x00000004,
    BATTLE_UNIT_STATUS_TREASURE = 0x00000008,
    BATTLE_UNIT_STATUS_TRANSPARENT = 0x00000010,
    BATTLE_UNIT_STATUS_FLOAT = 0x00000020,
    BATTLE_UNIT_STATUS_JUMP = 0x00000040,
    BATTLE_UNIT_STATUS_POACHED = 0x00000080,

    /* runtime +0x145. Other bits remain unknown. */
    BATTLE_UNIT_STATUS_MORBOL = 0x00000100,
    BATTLE_UNIT_STATUS_JUMP_HEIGHT_ACTIVE = 0x00000200,

    BATTLE_UNIT_STATUS_TRANSFORMATION_MASK = 0x0000000f,
} battle_unit_status_flags_5_6_e;

/* unit storage and rewards */
/* In-use 32-bit values at 0x800b7304, stride 0x440: each sits four bytes ahead of its
 * g_battle_units record, in the previous record's last four bytes. */
typedef struct battle_unit_slot_state {
    s32 in_use;
    u8 _unused_004[0x43c];
} battle_unit_slot_state_t;
typedef char battle_unit_slot_state_size_must_be_1088[(sizeof(battle_unit_slot_state_t) == 1088) ? 1 : -1];

/* Provisional: war-trophy search result filled by
 * battle_unit_find_war_trophies_and_bonus_money. REQUIRE links the same routine
 * as battle_unit_find_war_trophies_and_bonus_money into
 * g_require_reward_war_trophy_search_result. */
typedef struct battle_war_result {
    u8 item_ids[BATTLE_UNIT_SLOT_COUNT]; /* 0x00 */
    u8 item_count;                       /* 0x15 */
    u8 _padding_16[2];                   /* 0x16; aligns bonus_money */
    s32 bonus_money;                     /* 0x18; sum of bonus-money modifiers * 100 */
    s32 level_sum_money;                 /* 0x1c; g_battle_enemy_level_sum * 100 */
    s32 level_bonus_money;               /* 0x20; (highest enemy level - lowest level) * 100 */
} battle_war_result_t;
typedef char battle_war_result_size_must_be_36[(sizeof(battle_war_result_t) == 36) ? 1 : -1];

/* Seven result bytes; assigning elevation preserves the unassigned high bit. */
typedef struct battle_unit_height_data {
    u8 x;
    u8 y;
    u8 elevation : 7;
    u8 _unused_02 : 1; /* not read or assigned */
    u8 unit_flags;
    u8 unit_height;
    u8 walking_height;
    u8 total_height;
} battle_unit_height_data_t;
typedef char battle_unit_height_data_size_must_be_7[sizeof(battle_unit_height_data_t) == 7 ? 1 : -1];

/* Sprite lookup result filled in by battle_gfx_get_unit_sprite_frame_and_vram_ids. */
typedef struct {
    s16 clut;             /* 0x00; the unit's CLUT of MAP_CLUT_ROW_UNITS */
    s16 tpage;            /* 0x02 */
    s16 graphic_height;   /* 0x04 */
    s16 graphic_y_offset; /* 0x06 */
} battle_unit_sprite_query_t;

/* Facing values shifted to position bit 8 for the evasion comparisons. */
enum {
    BATTLE_UNIT_POSITION_FACING_SOUTH = 0x0000,
    BATTLE_UNIT_POSITION_FACING_WEST = 0x0100,
    BATTLE_UNIT_POSITION_FACING_NORTH = 0x0200,
    BATTLE_UNIT_POSITION_FACING_EAST = 0x0300,
};

typedef void (*battle_distortion_animation_handler_t)(battle_unit_t* unit);

/* unit records */
extern battle_unit_t g_battle_units[BATTLE_UNIT_RUNTIME_SLOT_COUNT];
extern battle_unit_slot_state_t g_battle_unit_slot_states[];
extern s32 g_battle_unit_graphics_load_complete;
extern battle_unit_t* g_battle_unit_list_head;

s32 battle_unit_find_at_tile(s32 x, s32 y, u32 level, s32 filters);
s32 battle_unit_draw_and_activate_by_runtime_id(u32 runtime_id);
s32 battle_unit_disable_remove(s32 battle_id);
s32 battle_unit_get_battle_id_by_runtime_id(u32 runtime_id);
battle_unit_t* battle_unit_get_casting(void);
battle_unit_t* battle_unit_get_by_battle_id(u16 battle_id);
battle_unit_t* battle_unit_get_by_runtime_id(u16 runtime_id);
s32 battle_unit_get_runtime_id_by_battle_id(u32 battle_id);
battle_unit_t* battle_unit_get_source(void);
s32 battle_unit_has_runtime_id(u32 runtime_id);
s32 battle_unit_move_to_head(u32 runtime_id);
s32 battle_unit_remove_by_runtime_id(u32 runtime_id);
s32 battle_unit_set_cannot_exist(s32 battle_id);
unit_stats_t* battle_unit_find_active_data_pointer(void);
action_command_t* battle_unit_get_action_command_by_battle_id(s32 battle_id);
battle_unit_t* battle_unit_find_overlapping_at_tile(battle_unit_t* self, s32 x, s32 y, s32 z);
action_command_t* battle_unit_get_runtime_action_command_by_battle_id(u32 battle_id);
void battle_unit_remove(battle_unit_t* unit);
void battle_unit_clear_all(void);
s32 battle_unit_find_slot_by_character_identity(u32 character_identity);

/* encounters and deployment */
extern s32 g_battle_entd_selection_mode; /* battle_entd_selection_e */

/* Count and coordinates of the tiles already handed out as dismount
 * destinations this action. */
extern s32 g_battle_relocated_unit_count;
extern battle_tile_coordinates_t g_battle_relocated_unit_coords[];
extern s16 g_battle_unit_entd_spritesheet_ids[];
extern s32 g_battle_unit_entd_spritesheet_count;

unit_stats_t* battle_unit_find_stats_by_entd_unit_id(s32 unit_id, s32* out_battle_id);
void battle_unit_load_entd_units(entd_encounter_t* entd_data, s32 mode);
s32 battle_unit_set_enemy_level_data_by_battle_id(u32 battle_id);
s32 battle_unit_init_party_member_battle_data(s32 unused0, s32 unused1, s32 unit_index);
s32 battle_unit_place_in_new_location(u32 runtime_id, map_tile_coordinates_t* tile, s32 facing);
void battle_unit_set_enemy_level_data(unit_stats_t* unit);
s32 battle_unit_find_relocation_tile(s32 unit_index, map_tile_coordinates_t* destination);
s32 battle_unit_build_deployed_units_data(unit_deployment_coordinates_t* formation, s32 unused_1);
void battle_unit_init_deployed_units_data_for_debug_red_team(unit_deployment_coordinates_t* formation, s32 unused_1);
void battle_unit_store_entd_flags_without_control_flag(battle_unit_t* unit);

/* statuses and mounts */
extern const u32 g_battle_unit_status_mask_by_handler_index[STATUS_COUNT + 1];

/* The statuses a rider can be inflicted with, one mask per status byte. */
extern u8 g_battle_rider_status_infliction_mask[STATUS_BYTE_COUNT];

void battle_unit_dismount_rider(battle_unit_t* unit);
void battle_unit_mount_rider_onto_unit(battle_unit_t* rider, battle_unit_t* mount);
void battle_unit_set_map_coords_after_death_dismount(battle_unit_t* unit);
battle_chocobo_mount_state_e battle_unit_get_chocobo_mount_state(const unit_stats_t* unit);
void battle_unit_mount_onto_pending_mount(battle_unit_t* unit);
s32 battle_unit_prepare_rider_dismount(u32 runtime_id, s32 direction, s32 jump_length);

/* battle_unit_animation_e of each item type's attack at a higher, a level and a lower target. */
extern u8 g_battle_weapon_attack_animations[][3];
extern u16 g_battle_enemy_level_sum;
extern u8 g_battle_highest_enemy_level;

battle_unit_action_restriction_e battle_unit_get_action_restriction(const unit_stats_t* unit);

/* animation */
extern battle_ability_animation_data_t g_battle_ability_animation_data[BATTLE_ABILITY_ANIMATION_COUNT];

/* Two battle_unit_animation_e for each set: the one that starts a charge and
 * the one held while the unit charges or performs; 0 for none. */
extern u8 g_battle_ability_charge_animation_sets[BATTLE_CHARGE_ANIMATION_SET_COUNT]
                                                [BATTLE_CHARGE_ANIMATION_VARIANT_COUNT];
extern battle_distortion_animation_handler_t
    g_battle_unit_distortion_animation_handlers[BATTLE_DISTORTION_ANIMATION_COUNT];
extern s32 g_battle_unit_palette_animation_offset;
extern s32 g_battle_unit_palette_animation_timer;

/* WalkToAnim: requested animation applied after movement stops; 0 means none. */
extern u16 g_unit_pending_animation_ids[];
extern u16 g_unit_was_moving_latch[]; /* a unit with a pending animation was moving on the last update */
extern s8 g_battle_unit_float_bob_offsets[24];

/* Event-script rotation state for each runtime unit slot. */
extern unit_animation_state_t g_unit_animation_states[];

void battle_unit_clear_distortion_animation_id_10(battle_unit_t* unit);
void battle_unit_clear_distortion_animation_id_11(battle_unit_t* unit);
void battle_unit_clear_facing_update_suppression(u32 runtime_id);
void battle_unit_decide_facing_direction(battle_unit_t* unit, s32 facing);
s32 battle_unit_animate_teleport_distortion(battle_unit_t* unit);
void battle_unit_dispatch_distortion_animation(battle_unit_t* unit);
void battle_unit_face_toward_cursor(battle_unit_t* unit);
s32 battle_unit_set_target_animation_by_runtime_id(s32 runtime_id);
void battle_unit_set_facing_update_suppression(u32 runtime_id);
s16 battle_unit_get_facing_quadrant_by_runtime_id(u32 runtime_id);
void battle_unit_set_animation_based_on_mounted_state(battle_unit_t* unit);
void battle_unit_set_animation_based_on_status(battle_unit_t* unit);
void battle_unit_set_idle_animation_for_movement(battle_unit_t* unit);
void battle_unit_set_level_up_animation(battle_unit_t* unit);
void unit_set_facing(s32 runtime_id, s32 facing);
void battle_unit_snap_all_facings_to_quadrants(void);
void battle_unit_start_ability_charge_animation(battle_unit_t* unit);
void battle_unit_start_ability_charge_hold_animation(battle_unit_t* unit);
void battle_unit_store_animation_facing(u32 animation, s32 facing, battle_unit_t* unit);
void battle_unit_store_coordinate_mount_animation_data(battle_unit_t* unit);
void battle_unit_update_and_animate_units(void);
void battle_unit_update_anim_display_for_all_targets(battle_unit_t* unit);
void battle_unit_update_post_command_animation_display(battle_unit_t* unit);
void battle_unit_update_animation_for_status_changes(battle_unit_t* unit);
void battle_unit_set_target_animation_from_attack_type(battle_unit_t* attacker, battle_unit_t* target);
s16 battle_unit_get_camera_facing_quadrant_by_battle_id(u32 battle_id);
void battle_unit_call_bow_hardcoding_by_runtime_id(u16 attacker_id, u16 target_id);
void battle_unit_call_set_animation_based_on_status(struct battle_unit* unit);
void battle_unit_clear_horizontal_flip_flag(u32 runtime_id);
void battle_unit_clear_movement_effect_suppression(u32 runtime_id);
void battle_unit_clear_palette_update_suppression(u32 runtime_id);
void battle_unit_face_towards_action_target(battle_unit_t* unit, battle_unit_t* target);
s32 battle_unit_get_facing_nibble_by_runtime_id(u32 runtime_id);
void battle_unit_select_attack_animation_for_ability(battle_unit_t* attacker, battle_unit_t* target);
void battle_unit_select_weapon_attack_animation(battle_unit_t* attacker, battle_unit_t* target);
s32 battle_unit_set_animation_from_facing_nibble(u32 runtime_id, s32 facing_nibble);
void battle_unit_set_horizontal_flip_flag(u32 runtime_id);
void battle_unit_set_mounted_animation_by_runtime_id(u32 runtime_id);
void battle_unit_set_movement_effect_suppression(u32 runtime_id);
void battle_unit_set_palette_update_suppression(u32 runtime_id);
void battle_unit_set_specific_animation_by_runtime_id(u32 runtime_id, u32 animation);
void battle_unit_set_status_animation_by_runtime_id(u32 runtime_id);
void battle_unit_store_animation_facing_movement_data(s32 animation, s32 facing, battle_unit_t* unit);

/* world coordinates */

SVECTOR* battle_unit_get_world_coords_by_runtime_id(u32 runtime_id);
SVECTOR* battle_unit_get_world_coords_by_battle_id(u32 battle_id);
void unit_copy_world_coords(u32 runtime_id, SVECTOR* world_coords);
void unit_copy_world_coords_to_vector(u32 runtime_id, VECTOR* destination);
void battle_unit_set_real_coords_from_world_coords(struct battle_unit* unit);
void battle_unit_set_world_coords_from_real_coords(struct battle_unit* unit);
void battle_unit_set_move_and_world_coords(battle_unit_t* unit);

/* position and display */
battle_unit_height_data_t* battle_unit_calculate_height_data(battle_unit_height_data_t* out, s32 unit_id);
battle_unit_t* battle_unit_find_other_chocobo_at_map_coords(s32 map_x, s32 map_y, s32 map_z);
void unit_project_to_screen(u32 runtime_id, SVECTOR* screen_coordinates);
battle_unit_t* battle_unit_find_selectable_at_tile(s32 map_x, s32 map_y, s32 map_z);
void battle_unit_update_display_by_runtime_id(s32 runtime_id);
void battle_unit_update_float_and_jump_height(battle_unit_t* unit);
void battle_unit_update_graphics_by_runtime_id(u32 runtime_id);
s32 battle_unit_set_tile_position(s32 battle_id, u8 map_x, u8 map_y, u8 map_z, u8 facing);
void unit_copy_map_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords);
void battle_unit_copy_draw_coords(SVECTOR* destination, const battle_unit_t* runtime);
void battle_unit_dismount_rider_and_update_display(battle_unit_t* rider);
SVECTOR* battle_unit_get_event_offset_by_runtime_id(u32 runtime_id);
s32 battle_unit_get_map_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords);
battle_unit_t* battle_unit_find_runtime_at_tile(s32 map_x, s32 map_y, s32 map_z);
s32 battle_unit_get_tile_center_and_height_by_battle_id(u32 battle_id, SVECTOR* coords);
s32 battle_unit_add_event_offset_by_runtime_id(u32 runtime_id, const SVECTOR* amount);

/* progression and pickups */
void battle_unit_apply_poach_morbol_transformation(battle_unit_t* unit);
u8 battle_unit_calculate_move_exp_jp_up_to_earn(const unit_stats_t* unit);
s32 battle_unit_count_crystals(void);
void battle_unit_find_war_trophies_and_bonus_money(battle_war_result_t* result);
battle_unit_t* battle_unit_get_crystal_or_treasure_at_map_coords(s32 map_x, s32 map_y, s32 map_z);
void battle_unit_learn_used_ability(unit_stats_t* unit);
void battle_unit_transform_into_morbol(unit_stats_t* unit);
s32 battle_unit_apply_action_rewards(s32 unit_index, action_reward_display_t* display);
s32 battle_unit_build_gained_exp_jp_level_job_level(unit_stats_t* unit, action_reward_display_t* rewards);
s32 battle_unit_learn_from_crystal(unit_stats_t* unit, s32 flags);
s32 battle_unit_load_job_level(unit_stats_t* unit, s32 job_id, s32* out_slot);
s32 battle_unit_store_jp_and_calculate_unlocked_jobs(s32 unit_index, s32 jp, s32 team, s32 slot);
crystal_pickup_result_t* battle_unit_generate_crystal_or_treasure(unit_stats_t* mover);

/* unit views */
extern image_load_parameters_t g_battle_unit_summary_portrait_image_params;

/* shared with the per-program event interpreter and dialog thread */
s32 unit_is_moving(s32 runtime_id);

#endif
