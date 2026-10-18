#ifndef ANIMATION_H
#define ANIMATION_H

/* BATTLE unit animations: SEQ sequence opcodes and the animation state, animation ids, the charge, attack and reaction
 * animations of actions, and the distortion motions (teleport, rise, settle). */

#include "fft/status.h"
#include "fft/unit.h"
#include "psx/types.h"

struct sprite_display;
struct sprite_unit_seq_table;
struct sprite_unit_shp_frame_tables;

/* sequences */
/* SEQ sequence bytes, as animation_run_unit_seq_script (unit sheets) and
 * animation_run_wep_eff_seq_script (weapon and effect sheets) read them.
 * A sequence is a run of two-byte frame instructions (an SHP frame, then the
 * frames to wait; a wait of 0 holds the frame for good) and of commands:
 * ANIMATION_SEQ_OP_PREFIX, an opcode and its operand bytes. A jump offset is a
 * signed byte counted from the command's own prefix byte, so -2 repeats the
 * frame instruction before it. UNKNOWN_XX commands only skip their operands. */
enum { ANIMATION_SEQ_OP_PREFIX = 0xff };

typedef enum animation_seq_opcode {
    /* 0xbe-0xd2 and 0xd4 are unit commands: a weapon or effect sequence that uses one is an error. */
    ANIMATION_SEQ_OP_STOP_POSITION_COPIES = 0xbe,
    ANIMATION_SEQ_OP_START_POSITION_COPIES = 0xbf, /* the trail of sprite_afterimage_draw */
    ANIMATION_SEQ_OP_WAIT_FOR_DISTORTION = 0xc0,   /* offset: jump while a distortion animation runs */
    ANIMATION_SEQ_OP_START_DISTORTION = 0xc1,      /* distortion animation id - 2, timer */
    ANIMATION_SEQ_OP_NOP = 0xc2,
    ANIMATION_SEQ_OP_HIDE_ITEM = 0xc3,
    ANIMATION_SEQ_OP_SET_ITEM_POSITION = 0xc4, /* x, y: signed offsets of the item sprite from the unit */
    ANIMATION_SEQ_OP_SHOW_ITEM = 0xc5,         /* draw the unit's item display (sprite_draw_item_display) */
    ANIMATION_SEQ_OP_WAIT_FOR_INPUT = 0xc6,    /* offset: jump while g_state_window_closed is 0 */
    ANIMATION_SEQ_OP_UNKNOWN_C7 = 0xc7,        /* two operands */
    ANIMATION_SEQ_OP_UNKNOWN_C8 = 0xc8,        /* two operands */
    ANIMATION_SEQ_OP_UNKNOWN_C9 = 0xc9,        /* two operands */
    ANIMATION_SEQ_OP_UNKNOWN_CA = 0xca,        /* one operand */
    ANIMATION_SEQ_OP_MOVE_UP_2 = 0xcb,         /* 0xcb-0xd2 shift the sprite by one or two units of seq_offset */
    ANIMATION_SEQ_OP_MOVE_UP_1 = 0xcc,
    ANIMATION_SEQ_OP_MOVE_BACKWARD_2 = 0xcd,
    ANIMATION_SEQ_OP_MOVE_BACKWARD_1 = 0xce,
    ANIMATION_SEQ_OP_MOVE_DOWN_2 = 0xcf,
    ANIMATION_SEQ_OP_MOVE_DOWN_1 = 0xd0,
    ANIMATION_SEQ_OP_MOVE_FORWARD_2 = 0xd1,
    ANIMATION_SEQ_OP_MOVE_FORWARD_1 = 0xd2,
    ANIMATION_SEQ_OP_WAIT_FOR_EFFECT_LOAD = 0xd3, /* offset: jump while g_state_animation_continue_check is 1 */
    ANIMATION_SEQ_OP_PLAY_WEAPON_SOUND = 0xd4,    /* 0 swing, 1 hit: action_play_weapon_sfx */
    ANIMATION_SEQ_OP_RESTART = 0xd5,              /* back to the sequence's first byte, counting the restart */
    ANIMATION_SEQ_OP_WAIT_FOR_EFFECT = 0xd6,      /* offset: jump while g_state_animation_continue_check is not 0 */
    ANIMATION_SEQ_OP_UNKNOWN_D7 = 0xd7,           /* one operand; the sequences of death and revival start with it */
    ANIMATION_SEQ_OP_SET_FRAME_BIAS = 0xd8,       /* signed; added to every later frame id */
    ANIMATION_SEQ_OP_LAUNCH_PROJECTILE = 0xd9, /* two unread operands: the thrown item, stone or arrow of the action */
    ANIMATION_SEQ_OP_UNDEFINED_DA = 0xda,      /* no handler: SYSTEM_ERROR_SEQ_OPCODE like any other value */
    ANIMATION_SEQ_OP_SET_WAIT_BIAS = 0xdb,     /* signed; added to every later wait */
    ANIMATION_SEQ_OP_RETURN = 0xdc,            /* resume the sequence ANIMATION_SEQ_OP_CALL_SEQUENCE left */
    ANIMATION_SEQ_OP_CALL_SEQUENCE = 0xdd,     /* sequence id: run it from its start; one level deep */
    ANIMATION_SEQ_OP_STRIKE_TARGETS = 0xde,    /* the blow of a weapon-like ability lands: its targets react */
    ANIMATION_SEQ_OP_CLEAR_ROTATION = 0xdf,
    ANIMATION_SEQ_OP_HIDE_SHADOW = 0xe0,
    ANIMATION_SEQ_OP_SHOW_SHADOW = 0xe1,
    ANIMATION_SEQ_OP_SET_LAYER_PRIORITY = 0xe2, /* row of g_animation_layer_priorities */
    ANIMATION_SEQ_OP_UNKNOWN_E3 = 0xe3,         /* one operand */
    ANIMATION_SEQ_OP_UNKNOWN_E4 = 0xe4,         /* one operand */
    ANIMATION_SEQ_OP_SET_ROTATION = 0xe5,       /* low byte, high byte: the display's rotation, ONE to the turn */
    ANIMATION_SEQ_OP_UNKNOWN_E6 = 0xe6,         /* two operands */
    ANIMATION_SEQ_OP_UNKNOWN_E7 = 0xe7,         /* two operands */
    ANIMATION_SEQ_OP_UNKNOWN_E8 = 0xe8,         /* two operands */
    ANIMATION_SEQ_OP_UNKNOWN_E9 = 0xe9,         /* one operand */
    ANIMATION_SEQ_OP_UNKNOWN_EA = 0xea,         /* two operands */
    ANIMATION_SEQ_OP_FLIP_VERTICAL = 0xeb,
    ANIMATION_SEQ_OP_FLIP_HORIZONTAL = 0xec,
    ANIMATION_SEQ_OP_UNKNOWN_ED = 0xed,   /* one operand */
    ANIMATION_SEQ_OP_MOVE_UNIT_FB = 0xee, /* signed: forward (+) or backward; 0xee-0xf0 and 0xfa move units only */
    ANIMATION_SEQ_OP_MOVE_UNIT_DU = 0xef, /* signed: down (+) or up */
    ANIMATION_SEQ_OP_MOVE_UNIT_RL = 0xf0, /* signed: right (+) or left */
    ANIMATION_SEQ_OP_UNKNOWN_F1 = 0xf1,   /* two operands */
    ANIMATION_SEQ_OP_START_BLOCK_SEQUENCE = 0xf2, /* sprite block 1-3, sequence id: show the weapon or effect sprite */
    ANIMATION_SEQ_OP_UNKNOWN_F3 = 0xf3,           /* one operand */
    ANIMATION_SEQ_OP_UNKNOWN_F4 = 0xf4,           /* one operand */
    ANIMATION_SEQ_OP_UNKNOWN_F5 = 0xf5,           /* one operand */
    ANIMATION_SEQ_OP_PLAY_SOUND = 0xf6,           /* sound_sfx_e */
    ANIMATION_SEQ_OP_UNKNOWN_F7 = 0xf7,           /* three operands */
    ANIMATION_SEQ_OP_UNKNOWN_F8 = 0xf8,           /* two operands */
    ANIMATION_SEQ_OP_MOVE_SCREEN_OFFSET = 0xf9,   /* signed x, y: added to the unit's screen_offset */
    ANIMATION_SEQ_OP_MOVE_UNIT = 0xfa,            /* right, down, forward: the operands of 0xf0, 0xef and 0xee */
    ANIMATION_SEQ_OP_UNKNOWN_FB = 0xfb,           /* two operands */
    ANIMATION_SEQ_OP_REPEAT = 0xfc,               /* offset, count: jump count times, then go on */
    ANIMATION_SEQ_OP_JUMP = 0xfd,                 /* offset */
    ANIMATION_SEQ_OP_END_ANIMATION = 0xfe,        /* stop; a weapon or effect sprite is hidden too */
    ANIMATION_SEQ_OP_PAUSE_ANIMATION = 0xff /* stop; either one puts a unit in its g_animation_targeted_frame_* pose */
} animation_seq_opcode_e;

/* Animation ids from 500 up are not doubled into a front and a back
 * sequence: 500 + n and 600 + n play sequence n of EVTCHR slot 0 and 1. */
enum { ANIMATION_SEQUENCE_EVTCHR_SLOT_0 = 500, ANIMATION_SEQUENCE_EVTCHR_SLOT_1 = 600 };

/* Unit sprite animation state passed to the sequence and frame renderers.
 * Weapon/effect sprite blocks share its first 0x1c bytes, with distinct tails. */
typedef struct animation_state {
    u16 trigger;      /* 0x00; nonzero while the sprite is drawn */
    u16 graphic_type; /* 0x02; sprite_graphic_type_e */
    /* 0x04; index of the sequence in the SEQ table. A unit's is its
     * animation_e * 2, + 1 when it faces away from the camera;
     * from ANIMATION_SEQUENCE_EVTCHR_SLOT_0 up it is a sequence of an EVTCHR slot. */
    u16 sequence_id;
    u16 script_pos;        /* 0x06; offset of the next byte to read in the sequence */
    u16 frame;             /* 0x08; SHP frame on display */
    u16 wait;              /* 0x0a; frames until the sequence is read again; 0 when it has stopped */
    u16 restart_count;     /* 0x0c; ANIMATION_SEQ_OP_RESTART commands run since the sequence started */
    u16 saved_sequence_id; /* 0x0e; where ANIMATION_SEQ_OP_RETURN resumes */
    u16 saved_script_pos;  /* 0x10 */
    u16 wait_bias;         /* 0x12; ANIMATION_SEQ_OP_SET_WAIT_BIAS: added to each wait read */
    u16 frame_bias;        /* 0x14; ANIMATION_SEQ_OP_SET_FRAME_BIAS: added to each frame id read */
    u16 repeat_count;      /* 0x16; jumps the ANIMATION_SEQ_OP_REPEAT in progress has left; 0 when none is */
    u16 flags;             /* 0x18; the flip bits of sprite_display_flags_e that the sequence toggled */
    u8 _padding_1a[2];     /* 0x1a; aligns shp */
    struct sprite_unit_shp_frame_tables* shp; /* 0x1c */
    struct sprite_unit_seq_table* seq;        /* 0x20 */
    struct sprite_unit_shp_frame_tables*
        saved_shp;                           /* 0x24; the sheet's own tables while a status sheet replaces them */
    struct sprite_unit_seq_table* saved_seq; /* 0x28 */
    struct sprite_display* display;          /* 0x2c */
} animation_state_t;
typedef char animation_state_size_must_be_48[sizeof(animation_state_t) == 48 ? 1 : -1];

/* unit animations */
/* Unit animation ids: the ones the code and the two animation tables of
 * BATTLE.BIN select by number. Animation n is sequences 2n (seen from the
 * front) and 2n + 1 (from behind) of the sheet's SEQ file; the comments
 * describe the sequences of the human sheets. */
typedef enum animation {
    /* Plays sequences 1 to 5 instead: the standing pose for each direction the unit is seen from. */
    ANIMATION_STAND = 2, /* stop, petrify, cursed; the default hit reaction */
    ANIMATION_IDLE = 3,
    ANIMATION_IDLE_HASTE = 4,
    ANIMATION_IDLE_SLOW = 5,
    ANIMATION_IDLE_FLYING = 6, /* sheets with the flying flag */
    ANIMATION_IDLE_FLYING_HASTE = 7,
    ANIMATION_IDLE_FLYING_SLOW = 8,
    ANIMATION_SUBMERGED_IDLE = 9, /* in water two or more deep (animation_get_water_mode); also the crystal sprite */
    ANIMATION_SUBMERGED_IDLE_HASTE = 10,
    ANIMATION_SUBMERGED_IDLE_SLOW = 11,
    ANIMATION_WALK = 12, /* a movement step on foot (animation_set_walk) */
    ANIMATION_WALK_FAST = 13,
    ANIMATION_WALK_SLOW = 14,
    ANIMATION_FLY = 18,      /* a flying unit's movement step */
    ANIMATION_TREASURE = 21, /* the treasure chest sprite */
    ANIMATION_KNEEL = 22,    /* one held frame, the picture of SLEEP: event poison, a failed mimic */
    ANIMATION_DEFEND = 23,
    ANIMATION_DODGE = 24,
    ANIMATION_HURT = 25,
    ANIMATION_DEAD = 26,
    ANIMATION_RAISE_HANDS = 27, /* healed, blade grasp or catch, crystal pickup */
    /* Jumps for joy; in water one deep and for a rider, small hops instead. */
    ANIMATION_LEVEL_UP = 28,
    ANIMATION_LEVEL_UP_HOP = 29,
    ANIMATION_JUMP = 30,
    ANIMATION_FALL = 31,
    ANIMATION_LAND = 32, /* after a jump or a fall */
    /* Held while charging: the second animation of several charge animation sets. */
    ANIMATION_CHARGE = 34,
    ANIMATION_CLIMB = 35,
    ANIMATION_SLEEP = 36, /* kneeling: also the critical status */
    ANIMATION_CONFUSION = 37,
    ANIMATION_SING = 40,
    ANIMATION_DANCE = 41,
    ANIMATION_CAST_START = 42,  /* the first animation of most charge animation sets */
    ANIMATION_CAST_CHARGE = 43, /* its loop, held while charging as CHARGE is */
    /* Arms raised until the effect ends: the usual attack animation of the
     * ability table, and every ability of the RUKA, ARUTE and KANZEN sequences. */
    ANIMATION_CAST = 44,
    ANIMATION_HOP = 48, /* short hops alternate between the two */
    ANIMATION_HOP_ALTERNATE = 49,
    ANIMATION_RIDING = 50, /* a mounted unit; the rider while its mount moves */
    ANIMATION_FOUND_ITEM = 51,
    ANIMATION_DEATH = 52,
    ANIMATION_REVIVE = 53,
    ANIMATION_HOLD_ITEM = 57,
    ANIMATION_TELEPORT = 60,
    /* attacks: HIGH at a target at least 12 higher, LOW at one at least 12 lower */
    ANIMATION_UNARMED_ATTACK_HIGH = 61,
    ANIMATION_UNARMED_ATTACK_LEVEL = 62,
    ANIMATION_UNARMED_ATTACK_LOW = 63,
    ANIMATION_SWING_HIGH = 64,
    ANIMATION_SWING_LEVEL = 65,
    ANIMATION_SWING_LOW = 66,
    ANIMATION_THROW_ITEM = 76, /* an item used on a target that is not adjacent */
    ANIMATION_THRUST_HIGH = 77,
    ANIMATION_THRUST_LEVEL = 78,
    ANIMATION_THRUST_LOW = 79,
    ANIMATION_GUN_HIGH = 80,
    ANIMATION_GUN_LEVEL = 81,
    ANIMATION_GUN_LOW = 82,
    ANIMATION_BOW_HIGH = 83, /* selected through projectile_get_bow_animation_column */
    ANIMATION_BOW_LEVEL = 84,
    ANIMATION_BOW_LOW = 85,
    ANIMATION_BOOK = 86,
    ANIMATION_INSTRUMENT = 87,
    ANIMATION_WEAPON_GUARD_HIGH = 88, /* the attacker stands at least 12 higher */
    ANIMATION_WEAPON_GUARD_LEVEL = 89,
    ANIMATION_WEAPON_GUARD_LOW = 90,
    ANIMATION_SWING_BLOCKED_HIGH = 91, /* the attacker's recoil from a blocked attack */
    ANIMATION_SWING_BLOCKED_LEVEL = 92,
    ANIMATION_SWING_BLOCKED_LOW = 93,
    ANIMATION_UNARMED_ATTACK_BLOCKED_HIGH = 94,
    ANIMATION_UNARMED_ATTACK_BLOCKED_LEVEL = 95,
    ANIMATION_UNARMED_ATTACK_BLOCKED_LOW = 96,
    ANIMATION_THRUST_BLOCKED_HIGH = 97,
    ANIMATION_THRUST_BLOCKED_LEVEL = 98,
    ANIMATION_THRUST_BLOCKED_LOW = 99,
    ANIMATION_JUMP_DEPART = 117, /* the jump status is added in event mode; charging a Jump */
    ANIMATION_JUMP_LAND = 118,
    ANIMATION_CLOTH_HIGH = 119, /* bags and cloths */
    ANIMATION_CLOTH_LEVEL = 120,
    ANIMATION_CLOTH_LOW = 121
} animation_e;

/* The teleport animation and the attack animations after it are ATTACK_FIRST and up. */
enum { ANIMATION_ATTACK_FIRST = ANIMATION_TELEPORT };

void animation_set_specific_by_runtime_id(u32 runtime_id, u32 animation);
void animation_store_facing(u32 animation, s32 facing, unit_t* unit);
void animation_force_facing(s32 animation, s32 facing, unit_t* unit);
void animation_set_walk(unit_t* unit);
void animation_set_level_up(unit_t* unit);
void animation_set_mounted(unit_t* unit);
void animation_set_mounted_by_runtime_id(u32 runtime_id);
void animation_set_for_status(unit_t* unit);
void animation_call_set_for_status(struct unit* unit);
void animation_set_status_by_runtime_id(u32 runtime_id);
void animation_update_for_status_changes(unit_t* unit);

/* action animations */
/* One row of the ability animation table, indexed by ability id: the ability's charge animation
 * set, its attack animation and the message shown when it hits. */
typedef struct animation_ability_data {
    u8 charge_animation_set_id; /* row of g_animation_ability_charge_sets */
    u8 attack_animation;        /* animation_e, or ANIMATION_ATTACK_USE_WEAPON or _USE_ITEM */
    u8 text_display; /* low byte of the message text id (0x1800 | this) shown when the ability hits; 0 none */
} animation_ability_data_t;
typedef char animation_ability_data_size_must_be_3[(sizeof(animation_ability_data_t) == 3) ? 1 : -1];

/* Rows of the ability animation table, and the charge animation sets with their two animations. */
enum { ANIMATION_ABILITY_COUNT = 454, ANIMATION_CHARGE_SET_COUNT = 20, ANIMATION_CHARGE_VARIANT_COUNT = 2 };

/* Selectors in attack_animation; other values are animation_e IDs. */
enum { ANIMATION_ATTACK_USE_WEAPON = 0, ANIMATION_ATTACK_USE_ITEM = 1 };

void animation_start_ability_charge(unit_t* unit);
void animation_start_ability_charge_hold(unit_t* unit);
void animation_select_attack_for_ability(unit_t* attacker, unit_t* target);
void animation_select_weapon_attack(unit_t* attacker, unit_t* target);

/* distortions */
/* unit_t.distortion_animation_id: the motion that
 * animation_distortion_dispatch steps once an animation frame
 * until its handler clears the id. ANIMATION_SEQ_OP_START_DISTORTION gives the
 * id less 2 and, for most, the frames it takes (distortion_timer). */
typedef enum animation_distortion {
    ANIMATION_DISTORTION_NONE = 0,
    ANIMATION_DISTORTION_SCALE_PULSE = 1,      /* squeezes the sprite thin and tall; nothing starts it */
    ANIMATION_DISTORTION_RISE = 2,             /* 0x24 up */
    ANIMATION_DISTORTION_SETTLE = 3,           /* back to the height of where the unit stands */
    ANIMATION_DISTORTION_MOVE_TO_TARGET = 4,   /* to the first target, or to the targeted panel */
    ANIMATION_DISTORTION_MOVE_TO_OWN_TILE = 5, /* to the center of the unit's own tile */
    ANIMATION_DISTORTION_JUMP_TO_TARGET = 6,   /* the same two along a jump arc */
    ANIMATION_DISTORTION_JUMP_TO_OWN_TILE = 7,
    ANIMATION_DISTORTION_MOVE_TO_TARGET_LEVEL = 8, /* 4 and 5 without the change of height */
    ANIMATION_DISTORTION_MOVE_TO_OWN_TILE_LEVEL = 9,
    ANIMATION_DISTORTION_CLEAR = 10, /* 10 and 11 clear the id at once */
    ANIMATION_DISTORTION_CLEAR_2 = 11,
    ANIMATION_DISTORTION_JUMP_START = 12,             /* straight up to 0x120 above the ground, trailing copies */
    ANIMATION_DISTORTION_FALL_TO_TARGET = 13,         /* down onto the target's tile */
    ANIMATION_DISTORTION_TELEPORT = 14,               /* fade out, move to the step's destination, fade in */
    ANIMATION_DISTORTION_BACK_AWAY = 15,              /* part of a tile away from the target */
    ANIMATION_DISTORTION_JUMP_TO_OWN_TILE_FADED = 16, /* 7, blended, and dark until four frames remain */
    ANIMATION_DISTORTION_JUMP_RISE = 17               /* 12 with the jump sound, unless the unit is that high already */
} animation_distortion_e;

void animation_distortion_dispatch(unit_t* unit);

#endif
