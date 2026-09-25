#include "fft/battle.h"
#include "psx/types.h"

/*
 * Snap a screen-space point to the 28-unit tile grid for the current camera
 * yaw, writing the result to output.
 *
 * Same shape as battle_camera_calculate_relative_offset: each of the x and z
 * components picks up the caller's offset with a sign that depends on the yaw
 * quadrant, and the y component is a plain bias. Here each component is first
 * snapped down to a whole tile, and on the side the camera faces away from it
 * is moved on by one further tile. Callers pass an offset of 1 or 5. The
 * far_edge temporaries keep the target's add-then-subtract order; written
 * inline, GCC re-associates them into a longer sequence.
 */
void battle_camera_convert_screen_coords_by_rotation(const u16* input, s16* output, s32 offset) {
    s32 yaw;

    output[1] = input[1] - 0xb;
    yaw = g_battle_camera_render_state.vy & 0xc00;

    switch (yaw) {
    case 0: {
        s32 far_edge = (s16)((s16)input[0] / 28) * 28 + 0x1c;
        output[0] = far_edge - offset;
    }
        output[2] = (s16)((s16)input[2] / 28) * 28 + offset;
        break;
    case 0x400: {
        s32 far_edge = (s16)((s16)input[0] / 28) * 28 + 0x1c;
        output[0] = far_edge - offset;
    }
        {
            s32 far_edge = (s16)((s16)input[2] / 28) * 28 + 0x1c;
            output[2] = far_edge - offset;
        }
        break;
    case 0x800:
        output[0] = (s16)((s16)input[0] / 28) * 28 + offset;
        {
            s32 far_edge = (s16)((s16)input[2] / 28) * 28 + 0x1c;
            output[2] = far_edge - offset;
        }
        break;
    case 0xc00:
        output[0] = (s16)((s16)input[0] / 28) * 28 + offset;
        output[2] = (s16)((s16)input[2] / 28) * 28 + offset;
        break;
    }
}
