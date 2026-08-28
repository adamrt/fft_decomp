#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* libgs GsSwapDispBuff counterpart: present the active buffer and flip.
 *
 * The frame counter is pre-incremented in place and its new value stored
 * again after skipping zero; the pre-increment keeps the target's register
 * copy between the increment and the first store. */
void world_gs_swapdispbuff(void) {
    s32 stored;

    g_world_gs_dispenv.disp.x = ((u16*)g_world_gs_buffer_x)[g_world_gs_active_buffer];
    g_world_gs_dispenv.disp.y = ((u16*)g_world_gs_buffer_y)[g_world_gs_active_buffer];
    PutDispEnv(&g_world_gs_dispenv);
    SetDispMask(1);

    stored = ++g_world_gs_frame_count;
    if (stored == 0) {
        stored = 1;
    }
    g_world_gs_frame_count = stored;
    g_world_gs_active_buffer = (g_world_gs_active_buffer == 0);
    world_gs_setdrawbuffclip();
    world_gs_setdrawbuffoffset();
}
