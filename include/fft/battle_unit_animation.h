#ifndef BATTLE_UNIT_ANIMATION_H
#define BATTLE_UNIT_ANIMATION_H

/* BATTLE unit animation: animation ids, ability and distortion animations, facing and display position. */

#include "fft/battle_unit.h"
#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* animation ids */
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
    ANIMATION_BOW_HIGH = 83, /* a bow shot takes its row from projectile_get_bow_shot_angle */
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
    ANIMATION_CLOTH_LOW = 121,
} animation_e;

/* The teleport animation and the attack animations after it are ATTACK_FIRST and up. */
enum { ANIMATION_ATTACK_FIRST = ANIMATION_TELEPORT };

/* ability animations */
/* One row of the ability animation table, indexed by ability id: the ability's charge animation
 * set, its attack animation and the message shown when it hits. */
typedef struct animation_ability_data {
    u8 charge_animation_set_id; /* row of g_animation_ability_charge_sets */
    u8 attack_animation;        /* animation_e, or ANIMATION_ATTACK_USE_WEAPON or _USE_ITEM */
    u8 text_display; /* low byte of the message text id (0x1800 | this) shown when the ability hits; 0 none */
} animation_ability_data_t;
typedef char battle_ability_animation_data_size_must_be_3[(sizeof(animation_ability_data_t) == 3) ? 1 : -1];

/* Rows of the ability animation table, and the charge animation sets with their two animations. */
enum {
    ANIMATION_ABILITY_COUNT = 454,
    ANIMATION_CHARGE_SET_COUNT = 20,
    ANIMATION_CHARGE_VARIANT_COUNT = 2,
};

/* Selectors in attack_animation; other values are animation_e IDs. */
enum {
    ANIMATION_ATTACK_USE_WEAPON = 0,
    ANIMATION_ATTACK_USE_ITEM = 1,
};

void animation_start_ability_charge(unit_t* unit);
void animation_start_ability_charge_hold(unit_t* unit);
void animation_select_attack_for_ability(unit_t* attacker, unit_t* target);
void animation_select_weapon_attack(unit_t* attacker, unit_t* target);
void action_resolve_projectile_hit_by_runtime_id(u16 attacker_id, u16 target_id);
void animation_set_target(unit_t* attacker, unit_t* target);
s32 animation_set_target_by_runtime_id(s32 runtime_id);

/* animation */
extern s32 g_cursor_tile_glow_cycle_offset;
extern s32 g_cursor_tile_glow_cycle_timer;
extern s8 g_unit_float_bob_offsets[24];

void animation_set_for_status(unit_t* unit);
void animation_call_set_for_status(struct unit* unit);
void animation_update_for_status_changes(unit_t* unit);
void animation_set_status_by_runtime_id(u32 runtime_id);
void animation_set_mounted(unit_t* unit);
void animation_set_mounted_by_runtime_id(u32 runtime_id);
void animation_set_walk(unit_t* unit);
void animation_set_level_up(unit_t* unit);
void animation_set_specific_by_runtime_id(u32 runtime_id, u32 animation);
s32 unit_facing_set_sixteenth_by_runtime_id(u32 runtime_id, s32 facing_nibble);
void animation_store_facing(u32 animation, s32 facing, unit_t* unit);
void animation_force_facing(s32 animation, s32 facing, unit_t* unit);
void unit_update_and_animate_all(void);
void unit_update_target_displays(unit_t* unit);
void action_end_command(unit_t* unit);
void move_settle_on_tile(unit_t* unit);

/* distortion animations */
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
    ANIMATION_DISTORTION_JUMP_RISE = 17,              /* 12 with the jump sound, unless the unit is that high already */
} animation_distortion_e;

enum {
    ANIMATION_DISTORTION_COUNT = 18, /* the handlers of g_animation_distortion_handlers */
};

typedef void (*animation_distortion_handler_t)(unit_t* unit);

extern animation_distortion_handler_t g_animation_distortion_handlers[ANIMATION_DISTORTION_COUNT];

void animation_distortion_dispatch(unit_t* unit);
s32 animation_distortion_scale_pulse(unit_t* unit);
void animation_distortion_clear(unit_t* unit);
void animation_distortion_clear_2(unit_t* unit);

/* facing */
void unit_facing_set_cardinal(unit_t* unit, s32 facing);
void unit_facing_turn_to_cursor(unit_t* unit);
void unit_facing_turn_to_target(unit_t* unit, unit_t* target);
s16 unit_facing_get_quadrant_by_runtime_id(u32 runtime_id);
s16 unit_facing_get_camera_quadrant_by_battle_id(u32 battle_id);
s32 unit_facing_get_sixteenth_by_runtime_id(u32 runtime_id);
void unit_facing_snap_all_to_quadrants(void);
void sprite_set_horizontal_flip_flag(u32 runtime_id);
void sprite_clear_horizontal_flip_flag(u32 runtime_id);

/* update suppression */
/* Set and clear the unit_motion_flags_e that suppress a unit's facing, movement-effect and
 * palette updates. */
void unit_set_facing_update_suppression(u32 runtime_id);
void unit_clear_facing_update_suppression(u32 runtime_id);
void unit_set_movement_effect_suppression(u32 runtime_id);
void unit_clear_movement_effect_suppression(u32 runtime_id);
void unit_set_palette_update_suppression(u32 runtime_id);
void unit_clear_palette_update_suppression(u32 runtime_id);

/* world coordinates */
SVECTOR* unit_get_world_coords_by_runtime_id(u32 runtime_id);
SVECTOR* unit_get_world_coords_by_battle_id(u32 battle_id);
void unit_set_real_coords_from_world_coords(struct unit* unit);
void unit_set_world_coords_from_real_coords(struct unit* unit);
void unit_set_move_and_world_coords(unit_t* unit);

/* position and display */
unit_height_data_t* unit_calculate_height_data(unit_height_data_t* out, s32 unit_id);
unit_t* unit_find_other_chocobo_at_map_coords(s32 map_x, s32 map_y, s32 map_z);
unit_t* unit_find_selectable_at_tile(s32 map_x, s32 map_y, s32 map_z);
void unit_update_display_by_runtime_id(s32 runtime_id);
void unit_update_float_and_jump_height(unit_t* unit);
void unit_update_graphics_by_runtime_id(u32 runtime_id);
s32 unit_set_tile_position(s32 battle_id, u8 map_x, u8 map_y, u8 map_z, u8 facing);
void unit_copy_draw_coords(SVECTOR* destination, const unit_t* runtime);
void unit_mount_dismount_rider_and_update_display(unit_t* rider);
SVECTOR* unit_get_event_offset_by_runtime_id(u32 runtime_id);
s32 unit_get_map_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords);
unit_t* unit_find_runtime_at_tile(s32 map_x, s32 map_y, s32 map_z);
s32 unit_get_tile_center_and_height_by_battle_id(u32 battle_id, SVECTOR* coords);
s32 unit_add_event_offset_by_runtime_id(u32 runtime_id, const SVECTOR* amount);

#endif
