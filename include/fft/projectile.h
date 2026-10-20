#ifndef PROJECTILE_H
#define PROJECTILE_H

/* Arrows, thrown items and guns: arcs and lines of fire, used by targeting, the AI and effects. */

#include "psx/libgte.h"
#include "psx/types.h"

s32 projectile_arc_check_between_units(u8 source_id, u8 target_id);
s32 projectile_arc_set_and_validate(s32 source_id, const SVECTOR* tile_position, s32 target_id);
s32 projectile_arc_trajectory_term_calculate(s32 launch_slope_q12, s32 progress);
s32 projectile_arc_launch_velocity_calculate(void);
s32 projectile_bow_animation_column_get(void);
s32 projectile_direct_check_between_units(u8 source_id, u8 target_id);
s32 projectile_direct_check_to_target(s32 source_id, const SVECTOR* tile_position, s32 target_id);
void projectile_arrow_trajectory_set(s32 source_id, const SVECTOR* tile_position, s32 target_id);
void projectile_trajectory_position_calculate(const VECTOR* src, s32 total, s32 current, VECTOR* dst);

#endif
