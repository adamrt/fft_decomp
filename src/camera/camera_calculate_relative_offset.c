#include "fft/battle_camera.h"
#include "psx/types.h"

void camera_calculate_relative_offset(const SVECTOR* input, SVECTOR* output, s32 offset) {
    u16 x;
    u16 z;

    output->vy = input->vy - 0xc;
    switch (g_camera_render_state.vy & 0xc00) {
    case 0:
        x = input->vx + 0xe;
        output->vx = x - offset;
        z = input->vz - 0xe;
        output->vz = z + offset;
        break;
    case 0x400:
        x = input->vx + 0xe;
        output->vx = x - offset;
        z = input->vz + 0xe;
        output->vz = z - offset;
        break;
    case 0x800:
        x = input->vx - 0xe;
        output->vx = x + offset;
        z = input->vz + 0xe;
        output->vz = z - offset;
        break;
    case 0xc00:
        x = input->vx - 0xe;
        output->vx = x + offset;
        z = input->vz - 0xe;
        output->vz = z + offset;
        break;
    }
}
