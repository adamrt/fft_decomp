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
    = 9, /* in water two or more deep (battle_move_get_water_animation_mode); also the crystal sprite */
    BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE_HASTE = 10,
    BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE_SLOW = 11,
    BATTLE_UNIT_ANIMATION_WALK = 12, /* a movement step on foot (battle_unit_set_walk_animation) */
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
    BATTLE_UNIT_ANIMATION_BOW_HIGH = 83, /* a bow shot takes its row from battle_effect_get_bow_shot_angle */
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

/* ability animations */
/* One row of the ability animation table, indexed by ability id: the ability's charge animation
 * set, its attack animation and the message shown when it hits. */
typedef struct battle_ability_animation_data {
    u8 charge_animation_set_id; /* row of g_battle_ability_charge_animation_sets */
    u8 attack_animation;        /* battle_unit_animation_e, or BATTLE_ATTACK_ANIMATION_USE_WEAPON or _USE_ITEM */
    u8 text_display; /* low byte of the message text id (0x1800 | this) shown when the ability hits; 0 none */
} battle_ability_animation_data_t;
typedef char battle_ability_animation_data_size_must_be_3[(sizeof(battle_ability_animation_data_t) == 3) ? 1 : -1];

/* Rows of the ability animation table, and the charge animation sets with their two animations. */
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

void battle_unit_start_ability_charge_animation(battle_unit_t* unit);
void battle_unit_start_ability_charge_hold_animation(battle_unit_t* unit);
void battle_unit_select_attack_animation_for_ability(battle_unit_t* attacker, battle_unit_t* target);
void battle_unit_select_weapon_attack_animation(battle_unit_t* attacker, battle_unit_t* target);
void battle_unit_call_bow_hardcoding_by_runtime_id(u16 attacker_id, u16 target_id);
void battle_unit_set_target_animation(battle_unit_t* attacker, battle_unit_t* target);
s32 battle_unit_set_target_animation_by_runtime_id(s32 runtime_id);

/* animation */
extern s32 g_battle_unit_palette_animation_offset;
extern s32 g_battle_unit_palette_animation_timer;
extern s8 g_battle_unit_float_bob_offsets[24];

void battle_unit_set_animation_based_on_status(battle_unit_t* unit);
void battle_unit_call_set_animation_based_on_status(struct battle_unit* unit);
void battle_unit_update_animation_for_status_changes(battle_unit_t* unit);
void battle_unit_set_status_animation_by_runtime_id(u32 runtime_id);
void battle_unit_set_mounted_animation(battle_unit_t* unit);
void battle_unit_set_mounted_animation_by_runtime_id(u32 runtime_id);
void battle_unit_set_walk_animation(battle_unit_t* unit);
void battle_unit_set_level_up_animation(battle_unit_t* unit);
void battle_unit_set_specific_animation_by_runtime_id(u32 runtime_id, u32 animation);
s32 battle_unit_set_animation_from_facing_nibble(u32 runtime_id, s32 facing_nibble);
void battle_unit_store_animation_facing(u32 animation, s32 facing, battle_unit_t* unit);
void battle_unit_force_animation_facing(s32 animation, s32 facing, battle_unit_t* unit);
void battle_unit_update_and_animate_units(void);
void battle_unit_update_target_displays(battle_unit_t* unit);
void battle_unit_end_command(battle_unit_t* unit);
void battle_unit_settle_on_tile(battle_unit_t* unit);

/* distortion animations */
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

typedef void (*battle_distortion_animation_handler_t)(battle_unit_t* unit);

extern battle_distortion_animation_handler_t
    g_battle_unit_distortion_animation_handlers[BATTLE_DISTORTION_ANIMATION_COUNT];

void battle_unit_dispatch_distortion_animation(battle_unit_t* unit);
s32 battle_unit_animate_teleport_distortion(battle_unit_t* unit);
void battle_unit_clear_distortion_animation_id_10(battle_unit_t* unit);
void battle_unit_clear_distortion_animation_id_11(battle_unit_t* unit);

/* facing */
void battle_unit_decide_facing_direction(battle_unit_t* unit, s32 facing);
void battle_unit_face_toward_cursor(battle_unit_t* unit);
void battle_unit_face_towards_action_target(battle_unit_t* unit, battle_unit_t* target);
s16 battle_unit_get_facing_quadrant_by_runtime_id(u32 runtime_id);
s16 battle_unit_get_camera_facing_quadrant_by_battle_id(u32 battle_id);
s32 battle_unit_get_facing_nibble_by_runtime_id(u32 runtime_id);
void battle_unit_snap_all_facings_to_quadrants(void);
void battle_unit_set_horizontal_flip_flag(u32 runtime_id);
void battle_unit_clear_horizontal_flip_flag(u32 runtime_id);

/* update suppression */
/* Set and clear the battle_motion_flags_e that suppress a unit's facing, movement-effect and
 * palette updates. */
void battle_unit_set_facing_update_suppression(u32 runtime_id);
void battle_unit_clear_facing_update_suppression(u32 runtime_id);
void battle_unit_set_movement_effect_suppression(u32 runtime_id);
void battle_unit_clear_movement_effect_suppression(u32 runtime_id);
void battle_unit_set_palette_update_suppression(u32 runtime_id);
void battle_unit_clear_palette_update_suppression(u32 runtime_id);

/* world coordinates */
SVECTOR* battle_unit_get_world_coords_by_runtime_id(u32 runtime_id);
SVECTOR* battle_unit_get_world_coords_by_battle_id(u32 battle_id);
void battle_unit_set_real_coords_from_world_coords(struct battle_unit* unit);
void battle_unit_set_world_coords_from_real_coords(struct battle_unit* unit);
void battle_unit_set_move_and_world_coords(battle_unit_t* unit);

/* position and display */
battle_unit_height_data_t* battle_unit_calculate_height_data(battle_unit_height_data_t* out, s32 unit_id);
battle_unit_t* battle_unit_find_other_chocobo_at_map_coords(s32 map_x, s32 map_y, s32 map_z);
battle_unit_t* battle_unit_find_selectable_at_tile(s32 map_x, s32 map_y, s32 map_z);
void battle_unit_update_display_by_runtime_id(s32 runtime_id);
void battle_unit_update_float_and_jump_height(battle_unit_t* unit);
void battle_unit_update_graphics_by_runtime_id(u32 runtime_id);
s32 battle_unit_set_tile_position(s32 battle_id, u8 map_x, u8 map_y, u8 map_z, u8 facing);
void battle_unit_copy_draw_coords(SVECTOR* destination, const battle_unit_t* runtime);
void battle_unit_dismount_rider_and_update_display(battle_unit_t* rider);
SVECTOR* battle_unit_get_event_offset_by_runtime_id(u32 runtime_id);
s32 battle_unit_get_map_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords);
battle_unit_t* battle_unit_find_runtime_at_tile(s32 map_x, s32 map_y, s32 map_z);
s32 battle_unit_get_tile_center_and_height_by_battle_id(u32 battle_id, SVECTOR* coords);
s32 battle_unit_add_event_offset_by_runtime_id(u32 runtime_id, const SVECTOR* amount);

/* event library */
/* WalkToAnim: requested animation applied after movement stops; 0 means none. */
extern u16 g_unit_pending_animation_ids[];
extern u16 g_unit_was_moving_latch[]; /* a unit with a pending animation was moving on the last update */
/* Event-script rotation state for each runtime unit slot. */
extern unit_animation_state_t g_unit_animation_states[];

void unit_set_facing(s32 runtime_id, s32 facing);
void unit_copy_world_coords(u32 runtime_id, SVECTOR* world_coords);
void unit_copy_world_coords_to_vector(u32 runtime_id, VECTOR* destination);
void unit_project_to_screen(u32 runtime_id, SVECTOR* screen_coordinates);
void unit_copy_map_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords);

#endif
