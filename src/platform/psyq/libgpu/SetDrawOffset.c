/* LIBGPU 80025460-800254a4. */
#include "psx/libgpu.h"

void SetDrawOffset(DR_OFFSET* packet, u16* position) {
    setlen(packet, PSYQ_GPU_PACKET_WORDS(DR_OFFSET));
    /* get_ofs converts the u16 inputs to signed 16-bit offsets. */
    packet->code[0] = get_ofs(position[0], position[1]);
    packet->code[1] = 0;
}
