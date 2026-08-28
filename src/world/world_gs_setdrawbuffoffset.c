#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Applies the current draw buffer's offsets. In offset draw mode the offset
 * moves into the draw environment and the GTE offsets are cleared; otherwise
 * the *other* buffer's offsets are programmed into the GTE and cached. */
void world_gs_setdrawbuffoffset(void) {
    s32 x;
    s32 y;

    if (g_world_gs_offset_draw_active != 0) {
        g_world_gs_offset_x = g_world_gs_offset_y = 0;
        g_world_gs_drawenv.ofs[0] = g_world_gs_ofs.vx + g_world_gs_buffer_x[g_world_gs_active_buffer];
        g_world_gs_drawenv.ofs[1] = g_world_gs_ofs.vy + g_world_gs_buffer_y[g_world_gs_active_buffer];
        PutDrawEnv(&g_world_gs_drawenv);
    } else {
        x = g_world_gs_ofs.vx + g_world_gs_buffer_x[g_world_gs_active_buffer != 0 ? 0 : 1];
        y = g_world_gs_ofs.vy + g_world_gs_buffer_y[g_world_gs_active_buffer != 0 ? 0 : 1];
        SetGeomOffset(x, y);
        g_world_gs_offset_x = x;
        g_world_gs_offset_y = y;
    }
}
