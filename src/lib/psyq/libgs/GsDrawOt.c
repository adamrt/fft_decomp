#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* Send an ordering table to the GPU, starting from its head tag. */
void GsDrawOt(GsOT* ot) {
    DrawOTag((u32*)ot->tag);
}
