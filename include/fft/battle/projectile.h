#ifndef PROJECTILE_H
#define PROJECTILE_H

/* Arrows, thrown items and guns: arcs and lines of fire, used by targeting, the AI and effects. */

#include "psx/libgte.h"
#include "psx/types.h"

/* trajectory state */
extern s32 g_projectile_launch_slope_q12;
extern SVECTOR g_projectile_origin;
extern SVECTOR g_projectile_destination;
extern s32 g_projectile_step_count;
extern s32 g_projectile_hit_unit_id; /* the unit the projectile hits, or -1 */

/* trajectory */
s32 projectile_arc_check_between_units(u8 source_id, u8 target_id);
s32 projectile_arc_set_and_validate(s32 source_id, const SVECTOR* tile_position, s32 target_id);
s32 projectile_arc_calculate_height(s32 launch_slope_q12, s32 progress);
s32 projectile_arc_calculate_launch_velocity(void);
s32 projectile_get_bow_animation_column(void);
s32 projectile_direct_check_between_units(u8 source_id, u8 target_id);
s32 projectile_direct_check_to_target(s32 source_id, const SVECTOR* tile_position, s32 target_id);
void projectile_set_arrow_trajectory(s32 source_id, const SVECTOR* tile_position, s32 target_id);
void projectile_direct_calculate_offset(const VECTOR* src, s32 total, s32 current, VECTOR* dst);

/* hit response */
void projectile_map_unit_start_hit_response(u16 attacker_id, u16 target_id);

#endif
