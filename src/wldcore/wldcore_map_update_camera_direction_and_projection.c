#include "fft/wldcore.h"
#include "psx/gte.h"
#include "psx/types.h"

typedef enum wldcore_projection_flags {
    WLDCORE_PROJECTION_FLAG_UPDATE_POSITION = 0x01,
    WLDCORE_PROJECTION_FLAG_UPDATE_DIRECTION = 0x02,
    WLDCORE_PROJECTION_FLAG_ALTERNATE_DIRECTION_SET = 0x04,
} wldcore_projection_flags_e;

/* Apply pending direction and GTE projection updates to the camera state.
 *
 * The direction word is the player marker's sprite_id: the facing octant picks
 * one of eight marker sprites from a base of 0x10, or 0x18 for the alternate
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
        direction = angle_direction + 0x10;
        if (flags & WLDCORE_PROJECTION_FLAG_ALTERNATE_DIRECTION_SET) {
            direction = angle_direction + 0x18;
        }
        if (state->marker.sprite_id != direction) {
            state->marker.sprite_id = direction;
            state->marker.anim_counter = 0;
            state->marker.frame_index = 0;
        }
        state->flags = flags ^ WLDCORE_PROJECTION_FLAG_UPDATE_DIRECTION;
    }

    if (g_wldcore_map_projection_state.flags & WLDCORE_PROJECTION_FLAG_UPDATE_POSITION) {
        RotTrans(&g_wldcore_map_projection_state.marker.position, &output, &transform_flag);
        g_wldcore_map_projection_state.marker.screen_x = output.vx;
        g_wldcore_map_projection_state.marker.screen_y = output.vy;
        g_wldcore_map_projection_state.flags ^= WLDCORE_PROJECTION_FLAG_UPDATE_POSITION;
    }
}
