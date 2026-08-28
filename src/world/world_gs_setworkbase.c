#include "fft/world.h"
#include "psx/types.h"

/* Sets the base of the packet work area used by the world GS packet
 * allocator (see world_gs_sortboxfill / world_ps_sort_sprite_bg). */
void world_gs_setworkbase(void* base) {
    g_world_gs_out_packet_p = base;
}
