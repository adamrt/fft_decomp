#include "fft/battle_camera.h"
#include "psx/types.h"

/* Offset a position for the current camera yaw: subtract 0xc from y, and move
 * x and z by half a tile (0xe) and by offset, with signs set by the quadrant. */
void battle_camera_calculate_relative_offset(const u16* input, s16* output, s32 offset) {
    s32 x;
    s32 z;

    output[1] = input[1] - 0xc;
    switch (g_battle_camera_render_state.vy & 0xc00) {
    case 0:
        x = input[0] + 0xe;
        output[0] = x - offset;
        z = input[2] - 0xe;
        output[2] = z + offset;
        break;
    case 0x400:
        x = input[0] + 0xe;
        output[0] = x - offset;
        z = input[2] + 0xe;
        output[2] = z - offset;
        break;
    case 0x800:
        x = input[0] - 0xe;
        output[0] = x + offset;
        z = input[2] + 0xe;
        output[2] = z - offset;
        break;
    case 0xc00:
        x = input[0] - 0xe;
        output[0] = x + offset;
        z = input[2] - 0xe;
        output[2] = z + offset;
        break;
    }
}
