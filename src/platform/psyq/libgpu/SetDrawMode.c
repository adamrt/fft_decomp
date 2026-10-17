/* LIBGPU 800254cc-80025524. */
#include "psx/libgpu.h"

void SetDrawMode(DR_MODE* packet, int dfe, int dtd, int tpage, RECT* window) {
    setlen(packet, PSYQ_GPU_PACKET_WORDS(DR_MODE));
    packet->code[0] = get_mode(dfe, dtd, tpage);
    packet->code[1] = get_tw(window);
}
