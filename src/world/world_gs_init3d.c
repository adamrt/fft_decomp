#include "fft/world.h"
#include "psx/types.h"

void world_gs_init3d(void) {
    g_world_gs_ofs.vx = (s16)(g_world_gs_screen_width / 2);
    g_world_gs_ofs.vy = (s16)(g_world_gs_screen_height / 2);
    world_gs_setdrawbuffoffset();
    D_801CD854 = 0xA;
    D_801CD83C = 0;
    D_801CD7DC = 0x3FFF;
}
/* padpadpadpadpadpadpadpadpadpadpadpadpadpadpadpadpadpadpadpadpadpadpadpadpad */
