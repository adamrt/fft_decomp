/* LIBGPU 800253a0-800253dc. */
#include "psx/libgpu.h"

void SetTexWindow(DR_TWIN* packet, RECT* window) {
    setlen(packet, PSYQ_GPU_PACKET_WORDS(DR_TWIN));
    packet->code[0] = get_tw(window);
    packet->code[1] = 0;
}
