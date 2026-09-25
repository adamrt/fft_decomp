#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/gs.h"
#include "psx/types.h"

/* The packet libgs sorts for a GsBOXF: a DR_TPAGE-style tag and draw-mode
 * word followed by the TILE body under the same tag. */
typedef struct {
    DR_TPAGE tpage; /* 0x00 */
    u8 r0;          /* 0x08 */
    u8 g0;          /* 0x09 */
    u8 b0;          /* 0x0A */
    u8 code;        /* 0x0B */
    s16 x0;         /* 0x0C */
    s16 y0;         /* 0x0E */
    u16 w;          /* 0x10 */
    u16 h;          /* 0x12 */
} world_gs_boxf_packet_t;

void world_gs_sortboxfill(GsBOXF* bp, void* otp, s32 pri) {
    world_gs_boxf_packet_t* packet;
    s32 attr;
    u32 hi;
    u32 lo;

    attr = bp->attribute;
    if (attr >= 0) {
        packet = g_world_gs_out_packet_p;
        hi = (attr >> 17) & 0x180;
        lo = ((attr >> 23) & 0x60) | 0xE1000200;
        packet->tpage.code[0] = hi | lo;
        packet->r0 = bp->r;
        packet->g0 = bp->g;
        packet->b0 = bp->b;
        packet->code = ((attr >> 29) & 2) | 0x60;
        packet->x0 = bp->x + g_world_gs_offset_x;
        packet->y0 = bp->y + g_world_gs_offset_y;
        packet->w = bp->w;
        packet->h = bp->h;
        /* The target truncates pri to 16 bits at the call. */
        g_world_gs_out_packet_p = (void*)world_ps_sort_sprite_bg((u32*)packet, otp, (u16)pri, 4);
    }
}
