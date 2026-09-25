#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/gs.h"
#include "psx/types.h"

/* The packet libgs sorts for a GsLINE: one four-word tag covering a
 * texture-page/draw-mode word and the LINE_F2 body, so it opens like a
 * DR_TPAGE and continues with the line's colour, code and endpoints. */
typedef struct {
    DR_TPAGE tpage; /* 0x00 */
    u8 r0;          /* 0x08 */
    u8 g0;          /* 0x09 */
    u8 b0;          /* 0x0A */
    u8 code;        /* 0x0B */
    s16 x0;         /* 0x0C */
    s16 y0;         /* 0x0E */
    s16 x1;         /* 0x10 */
    s16 y1;         /* 0x12 */
} world_gs_line_packet_t;

/* libgs GsSortLine: build a flat-shaded LINE_F2 packet for the line and link
 * it into the ordering table.
 *
 * `blue` has to be read out of the
 * GsLINE before the `prim->code` store. sched2 cannot prove that a load from
 * `line` does not alias a store to `prim`, so with `prim->b0 = line->b;`
 * written after the code store the blue load is pinned below it and the
 * load-delay slot fills with a nop; reading it into a local first lets the
 * scheduler put it between the `andi` and the `ori`, which is where the target
 * has it. */
void world_gs_sortline(GsLINE* line, GsOT* ot, u16 z) {
    world_gs_line_packet_t* prim;
    s32 attr;
    u8 blue;

    attr = line->attribute;
    if (attr >= 0) {
        prim = g_world_gs_out_packet_p;
        prim->tpage.code[0] = ((attr >> 23) & 0x60) | 0xE1000200;
        prim->r0 = line->r;
        prim->g0 = line->g;
        blue = line->b;
        prim->code = ((attr >> 29) & 2) | 0x40;
        prim->b0 = blue;
        prim->x0 = line->x0 + g_world_gs_offset_x;
        prim->y0 = line->y0 + g_world_gs_offset_y;
        prim->x1 = line->x1 + g_world_gs_offset_x;
        prim->y1 = line->y1 + g_world_gs_offset_y;
        g_world_gs_out_packet_p = (void*)world_ps_sort_sprite_bg((u32*)prim, ot, z, 4);
    }
}
