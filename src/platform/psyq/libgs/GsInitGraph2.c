#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* Reapply the display/draw mode without resetting the GPU: unlike
 * GsInitGraph it only restages the environments and refreshes the screen
 * state. */
void GsInitGraph2(u16 x, u16 y, u16 intmode, u16 dither, u16 vram) {
    GsDISPENV.disp.w = x;
    GsDISPENV.disp.h = y;
    GsDRAWENV.tpage = 0;
    GsDRAWENV.dtd = dither;
    GsDRAWENV.dfe = 0;
    GsDRAWENV.isbg = 0;
    GsDISPENV.isinter = intmode & GsINTER;
    PSDGPU = intmode & GsOFSGPU;
    GsDISPENV.isrgb24 = vram;
    psyq_gs_reset_screen_state(x, y);
}
