#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* The GPU half of GsInitGraph: reset, then the draw and display environments. */
void psyq_gs_init_gpu(u16 x, u16 y, u16 intmode, u16 dither, u16 vram) {
    ResetGraph(((intmode >> 4) & 3) == 3 ? 3 : 0);
    GsDRAWENV.ofs[0] = GsDRAWENV.ofs[1] = 0;
    GsDRAWENV.tw.w = GsDRAWENV.tw.h = 0;
    GsDRAWENV.tw.x = GsDRAWENV.tw.y = 0;
    GsDRAWENV.tpage = 0;
    GsDRAWENV.dtd = dither;
    GsDRAWENV.dfe = 0;
    GsDRAWENV.isbg = 0;
    PutDrawEnv(&GsDRAWENV);
    setRECT(&GsDISPENV.disp, 0, 0, x, y);
    setRECT(&GsDISPENV.screen, 0, 0, 0, 0);
    if (GetVideoMode() == MODE_PAL) {
        GsDISPENV.screen.y = 0x18;
        GsDISPENV.pad0 = MODE_PAL;
    }
    GsDISPENV.isinter = intmode & GsINTER;
    PSDGPU = intmode & GsOFSGPU;
    GsDISPENV.isrgb24 = vram;
    PutDispEnv(&GsDISPENV);
}
