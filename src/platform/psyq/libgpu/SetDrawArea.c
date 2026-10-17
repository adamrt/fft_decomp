/* LIBGPU 800253dc-80025460. */
#include "psx/libgpu.h"

void SetDrawArea(DR_AREA* packet, RECT* rect) {
    setlen(packet, PSYQ_GPU_PACKET_WORDS(DR_AREA));
    packet->code[0] = get_cs(rect->x, rect->y);
    packet->code[1] = get_ce((rect->x + rect->w - 1), (rect->y + rect->h - 1));
}
