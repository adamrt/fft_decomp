#include "fft/wldcore.h"
#include "psx/types.h"

/*
 * WORLD-map graphics-subsystem bring-up: display mask off, GS graph init at
 * 256x240, draw offsets/3D init, GTE screen offset, then the two double-
 * buffered ordering-table descriptors are pointed at their tag arrays.
 *
 * The descriptor stride of 0x14 matches the libgs GsOT record already assumed
 * by src/wldcore/wldcore_init_core.c, which indexes these same globals
 * with `g_active_graphics_buffer_index * 0x14`.
 */

void wldcore_gfx_init_subsystem(void) {
    SetDispMask(0);
    world_gs_set_display_mode(256, 240, 4, 0, 0);
    world_gs_set_draw_offsets(0, 0, 0, 240);
    world_gs_init3d();
    SetGeomOffset(0, 0);
    wldcore_clear_screen_and_set_map_clip_rect(0);

    g_wldcore_gfx_aux_ordering_tables[1].log2n = 2;
    g_wldcore_gfx_aux_ordering_tables[0].log2n = 2;
    g_wldcore_gfx_aux_ordering_tables[0].org = g_wldcore_gfx_aux_ot_tags[0];
    g_wldcore_gfx_aux_ordering_tables[1].org = g_wldcore_gfx_aux_ot_tags[1];

    g_wldcore_gfx_ordering_tables[1].log2n = 4;
    g_wldcore_gfx_ordering_tables[0].log2n = 4;
    g_wldcore_gfx_ordering_tables[0].org = g_wldcore_gfx_ot_tags[0];
    g_wldcore_gfx_ordering_tables[1].org = g_wldcore_gfx_ot_tags[1];
}
