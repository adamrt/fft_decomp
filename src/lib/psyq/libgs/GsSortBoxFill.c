#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* Build a flat-colored TILE with its draw-mode word from the box attributes, offset by the 2D
 * origin, and link it into the ordering table at pri; boxes with the display-off attribute are
 * skipped. */
void GsSortBoxFill(GsBOXF* bp, GsOT* otp, u16 pri) {
    TILE* packet;
    DR_TPAGE* draw_mode;
    s32 attr = bp->attribute; /* Attribute packing uses signed right shifts. */
    u32 hi;
    u32 lo;

    if (!(attr & GsDOFF)) {
        draw_mode = (DR_TPAGE*)GsOUT_PACKET_P;
        /* Share one DMA tag: the 32-bit mode command occupies this primitive view's tag slot. */
        packet = (TILE*)draw_mode->code;
        hi = (attr >> 17) & 0x180;
        lo = ((attr >> 23) & 0x60) | (PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_MODE) | 0x200);
        draw_mode->code[0] = hi | lo;
        setRGB0(packet, bp->r, bp->g, bp->b);
        packet->code = ((attr >> 29) & PSYQ_GPU_CODE_SEMI_TRANSPARENT) | PSYQ_GPU_CODE_TILE;
        setXY0(packet, bp->x + GsORGOFSX, bp->y + GsORGOFSY);
        setWH(packet, bp->w, bp->h);
        GsOUT_PACKET_P = psyq_gs_sort_sprite_bg((u32*)draw_mode, otp, pri, PSYQ_GPU_PACKET_WORDS(TILE) + 1);
    }
}
