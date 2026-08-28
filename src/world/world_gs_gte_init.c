#include "fft/world.h"
#include "psx/types.h"

void world_gs_gte_init(void) {
    InitGeom();
    SetFarColor(0, 0, 0);
    SetGeomOffset(0, 0);
    g_world_gs_offset_y = 0;
    g_world_gs_offset_x = 0;
}
