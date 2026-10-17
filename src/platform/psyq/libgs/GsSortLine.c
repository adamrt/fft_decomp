#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* Build and link the flat-shaded line packet. */
void GsSortLine(GsLINE* line, GsOT* ot, u16 z) {
    LINE_F2* prim;
    DR_TPAGE* draw_mode;
    s32 attr = line->attribute; /* Attribute packing uses signed right shifts. */

    if (!(attr & GsDOFF)) {
        draw_mode = (DR_TPAGE*)GsOUT_PACKET_P;
        /* Share one DMA tag: the 32-bit mode command occupies this primitive view's tag slot. */
        prim = (LINE_F2*)draw_mode->code;
        draw_mode->code[0] = ((attr >> 23) & 0x60) | (PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_MODE) | 0x200);
        setRGB0(prim, line->r, line->g, line->b);
        prim->code = ((attr >> 29) & PSYQ_GPU_CODE_SEMI_TRANSPARENT) | PSYQ_GPU_CODE_LINE_F2;
        setXY2(prim, line->x0 + GsORGOFSX, line->y0 + GsORGOFSY, line->x1 + GsORGOFSX, line->y1 + GsORGOFSY);
        GsOUT_PACKET_P = psyq_gs_sort_sprite_bg((u32*)draw_mode, ot, z, PSYQ_GPU_PACKET_WORDS(LINE_F2) + 1);
    }
}
