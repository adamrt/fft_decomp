#include "fft/battle_camera.h"
#include "psx/types.h"

/*
 * Snap a screen-space point to the 28-unit tile grid for the current camera
 * yaw, writing the result to output.
 *
 * Same shape as camera_calculate_relative_offset: each of the x and z
 * components picks up the caller's offset with a sign that depends on the yaw
 * quadrant, and the y component is a plain bias. Here each component is first
 * snapped down to a whole tile, and on the side the camera faces away from it
 * is moved on by one further tile. Callers pass an offset of 1 or 5. The
 * far_edge temporaries keep the target's add-then-subtract order; written
 * inline, GCC re-associates them into a longer sequence.
 */
void camera_convert_screen_coords_by_rotation(const SVECTOR* input, SVECTOR* output, s32 offset) {
    s32 yaw;

    output->vy = input->vy - 0xb;
    yaw = g_camera_render_state.vy & 0xc00;

    switch (yaw) {
    case 0: {
        s32 far_edge = (input->vx / MAP_TILE_SIZE) * MAP_TILE_SIZE + MAP_TILE_SIZE;
        output->vx = far_edge - offset;
    }
        output->vz = (input->vz / MAP_TILE_SIZE) * MAP_TILE_SIZE + offset;
        break;
    case 0x400: {
        s32 far_edge = (input->vx / MAP_TILE_SIZE) * MAP_TILE_SIZE + MAP_TILE_SIZE;
        output->vx = far_edge - offset;
    }
        {
            s32 far_edge = (input->vz / MAP_TILE_SIZE) * MAP_TILE_SIZE + MAP_TILE_SIZE;
            output->vz = far_edge - offset;
        }
        break;
    case 0x800:
        output->vx = (input->vx / MAP_TILE_SIZE) * MAP_TILE_SIZE + offset;
        {
            s32 far_edge = (input->vz / MAP_TILE_SIZE) * MAP_TILE_SIZE + MAP_TILE_SIZE;
            output->vz = far_edge - offset;
        }
        break;
    case 0xc00:
        output->vx = (input->vx / MAP_TILE_SIZE) * MAP_TILE_SIZE + offset;
        output->vz = (input->vz / MAP_TILE_SIZE) * MAP_TILE_SIZE + offset;
        break;
    }
}
