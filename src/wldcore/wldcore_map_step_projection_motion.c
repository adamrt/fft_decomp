#include "fft/wldcore.h"
#include "psx/gte.h"
#include "psx/types.h"

enum {
    WLDCORE_PROJECTION_MOTION_ACTIVE = 0x01,
};

/* These volatile field aliases preserve the target's independent load order.
 * Direct accesses through g_wldcore_map_projection_motion let GCC 2.6.3
 * combine/reorder the loads and do not reproduce this function's bytes. */
extern volatile s32 g_wldcore_map_projection_motion_progress;
extern volatile s32 g_wldcore_map_projection_motion_origin_x;
extern volatile s32 g_wldcore_map_projection_motion_target_x;

/* Advance one frame of the wrapped projection move started at 0x8008ec38.
 *
 * Interpolates the live projection origin between the snapshotted origin and
 * the clamped target by progress/distance. While progress is still short of
 * distance it is advanced and 1 is returned. On the final frame the origin is
 * snapped to the target, the record's active flag is toggled off, and 0 is
 * returned.
 *
 * The final snap goes through the VECTOR view of the target, which drops the
 * clamped-point volatile qualifier and lets the four words copy as one batched
 * struct assignment. */
s32 wldcore_map_step_projection_motion(void) {
    s32 progress;
    s32 distance;
    s32 target_x;
    s32 origin_x;
    s32 delta_x;
    s32 origin_y;

    target_x = g_wldcore_map_projection_motion_target_x;
    origin_x = g_wldcore_map_projection_motion_origin_x;
    progress = g_wldcore_map_projection_motion_progress;
    distance = g_wldcore_map_projection_motion.distance;
    target_x = (target_x - origin_x) * progress;
    delta_x = target_x / distance;
    g_wldcore_map_projection_origin.vx = origin_x + delta_x;
    origin_y = g_wldcore_map_projection_motion.origin.vy;
    g_wldcore_map_projection_origin.vy
        = origin_y + (g_wldcore_map_projection_motion.target.y - origin_y) * progress / distance;
    if (progress >= distance) {
        g_wldcore_map_projection_origin = *(VECTOR*)&g_wldcore_map_projection_motion.target;
        g_wldcore_map_projection_motion.flags ^= WLDCORE_PROJECTION_MOTION_ACTIVE;
        return 0;
    }
    g_wldcore_map_projection_motion.progress = progress + 1;
    return 1;
}
