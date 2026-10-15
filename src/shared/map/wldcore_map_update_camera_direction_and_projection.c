#include "fft/wldcore.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Apply pending direction and GTE projection updates to the camera state.
 *
 * The 32-bit direction value is the player marker's sequence: the facing octant picks
 * one of eight marker sequences from a base of 0x10, or 0x18 for the alternate
 * set. The projected point is written into the marker's screen_x/screen_y by
 * RotTrans, whose input is the marker's own position. */
void wldcore_map_update_camera_direction_and_projection(void) {
    VECTOR output;
    wldcore_projection_state_t* state;
    long transform_flag;
    s32 angle_direction;
    s32 direction;
    s32 flags;

    state = &g_wldcore_map_projection_state;
    flags = state->flags;
    if (flags & WLDCORE_PROJECTION_FLAG_UPDATE_DIRECTION) {
        angle_direction = ((state->angle + 0x100) >> 9) & 7;
        direction = angle_direction + WLDCORE_ANIM_SEQUENCE_MARKER_FIRST;
        if (flags & WLDCORE_PROJECTION_FLAG_ALTERNATE_DIRECTION_SET) {
            direction = angle_direction + WLDCORE_ANIM_SEQUENCE_MARKER_ALTERNATE_FIRST;
        }
        if (state->marker.render.sequence != direction) {
            state->marker.render.sequence = direction;
            state->marker.render.anim_counter = 0;
            state->marker.render.frame_index = 0;
        }
        state->flags = flags ^ WLDCORE_PROJECTION_FLAG_UPDATE_DIRECTION;
    }

    if (g_wldcore_map_projection_state.flags & WLDCORE_PROJECTION_FLAG_UPDATE_POSITION) {
        RotTrans(&g_wldcore_map_projection_state.marker.position, &output, &transform_flag);
        g_wldcore_map_projection_state.marker.render.position.x = output.vx;
        g_wldcore_map_projection_state.marker.render.position.y = output.vy;
        g_wldcore_map_projection_state.flags ^= WLDCORE_PROJECTION_FLAG_UPDATE_POSITION;
    }
}
