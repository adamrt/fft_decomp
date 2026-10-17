#ifndef PROJECTILE_H
#define PROJECTILE_H

/* Arrows, thrown items and guns: arcs and lines of fire, used by targeting, the AI and effects. */

#include "fft/battle_map.h"
#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Arc endpoint handed to the per-arc obstacle test: the unit's raised tile position followed by its id and height
 * allowance. */
typedef struct projectile_arc_endpoint {
    SVECTOR position;
    s32 unit_id;
    s32 height;
} projectile_arc_endpoint_t;

/* Offset to the target plus its fixed-point horizontal length. */
typedef struct projectile_arc_delta {
    s32 x;
    s32 y;
    s32 z;
    s32 distance;
} projectile_arc_delta_t;

/* Query read by projectile_arc_calculate_trajectory: the offset to the target and both endpoints. */
typedef struct projectile_arc_query {
    projectile_arc_delta_t delta;
    projectile_arc_endpoint_t source;
    projectile_arc_endpoint_t target;
} projectile_arc_query_t;

/* Unit list built in the scratchpad at 0x1f800000 by projectile_list_obstacle_units and read by the obstacle tests. */
typedef struct projectile_obstacle_unit_list {
    s32 count;                           /* 0x00 */
    u8 battle_ids[UNIT_SLOT_COUNT];      /* 0x04 */
    u8 _padding_19;                      /* 0x19; aligns the position vectors */
    SVECTOR positions[UNIT_SLOT_COUNT];  /* 0x1a; tile center and height */
    u8 _padding_c2[2];                   /* 0xc2; aligns the sprite heights */
    s32 sprite_heights[UNIT_SLOT_COUNT]; /* 0xc4 */
} projectile_obstacle_unit_list_t;
typedef char projectile_obstacle_unit_list_size_must_be_280[sizeof(projectile_obstacle_unit_list_t) == 280 ? 1 : -1];

extern s32 g_projectile_gravity_modifier;
extern s32 g_projectile_gravity_scale;
extern s32 g_projectile_launch_slope_q12;
extern s16 g_projectile_arctan_angle_mod;
extern s32 g_projectile_source_id;
extern SVECTOR g_projectile_origin;
extern SVECTOR g_projectile_destination;
extern s32 g_projectile_step_count;
extern VECTOR g_projectile_step;      /* last step's movement */
extern SVECTOR g_projectile_position; /* final projectile position */
extern s32 g_projectile_hit_unit_id;
extern battle_map_tile_coordinates_t g_projectile_tile;
extern s32 g_projectile_tile_flags;

s32 projectile_arc_check_between_units(u8 source_id, u8 target_id);
s32 projectile_arc_set_and_validate(s32 source_id, SVECTOR* tile_position, s32 target_id);
s32 projectile_get_bow_shot_angle(void);
s32 projectile_direct_check_between_units(u8 source_id, u8 target_id);
s32 projectile_direct_check_to_target(s32 source_id, SVECTOR* tile_position, s32 target_id);
void projectile_set_arrow_trajectory(s32 source_id, SVECTOR* tile_position, s32 target_id);

#endif
