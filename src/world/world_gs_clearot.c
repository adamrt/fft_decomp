#include "psx/gs.h"
#include "psx/types.h"

void world_gs_clearot(s32 x, s32 y, GsOT* ot) {
    ot->offset = x & 0xFFFF;
    ot->point = y & 0xFFFF;
    ot->tag = (u32*)((s32)ot->org + (4 << ot->log2n) - 4);
    ClearOTagR(ot->org, 1 << ot->log2n);
}
