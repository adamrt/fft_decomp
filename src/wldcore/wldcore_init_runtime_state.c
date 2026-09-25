#include "fft/wldcore.h"

/* Initialize runtime tables and graphics state used by WLDCORE.
 * The matched range includes the jr delay slot at 0x800683f8. */
void wldcore_init_runtime_state(void) {
    volatile s32 reserved_stack_words[2];
    s32 count;
    SetDispMask(0);
    g_wldcore_map_projection_state.marker.sub_kind
        = wldcore_proposition_load_picture(g_wldcore_map_projection_state.marker.kind);
    wldcore_wait_for_file_load();
    count = world_gs_get_active_buffer();
    g_active_graphics_buffer_index = count;
    world_gs_setworkbase(&g_wldcore_gfx_world_primitive_buffers + count * 0xE000);
    wldcore_init_core();
    VSync(0);
    ResetGraph(1);
    world_gs_clear_ot(0, 0, &g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    world_gs_clear_ot(0, 0, &g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_gfx_clear_vram_and_scratch(0);
    world_gs_swapdispbuff();
    SetDispMask(0);
}
