#ifndef PROJECTILE_H
#define PROJECTILE_H

/* Arrows, thrown items and guns: arcs and lines of fire, used by targeting, the AI and effects. */

#include "psx/libgte.h"
#include "psx/types.h"

s32 projectile_arc_check_between_units(u8 source_id, u8 target_id);
s32 projectile_arc_set_and_validate(s32 source_id, const SVECTOR* tile_position, s32 target_id);
s32 projectile_get_bow_animation_column(void);
s32 projectile_direct_check_between_units(u8 source_id, u8 target_id);
s32 projectile_direct_check_to_target(s32 source_id, const SVECTOR* tile_position, s32 target_id);
void projectile_set_arrow_trajectory(s32 source_id, const SVECTOR* tile_position, s32 target_id);

#endif
