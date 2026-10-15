#include "fft/wldcore.h"

/* Initialize runtime tables and graphics state used by WLDCORE.
 * The matched range includes the jr delay slot at 0x800683f8. */
void wldcore_init_runtime_state(void) {
    s32 reserved_stack_words[2];
    s32 count;
    SetDispMask(0);
    g_wldcore_map_projection_state.marker.sub_kind
        = wldcore_proposition_load_picture(g_wldcore_map_projection_state.marker.kind);
    file_wait_for_pending_variant();
    count = GsGetActiveBuff();
    g_active_graphics_buffer_index = count;
    GsSetWorkBase(g_wldcore_gfx_world_primitive_buffers + count * 0xE000);
    wldcore_init_core();
    VSync(0);
    ResetGraph(1);
    GsClearOt(0, 0, &g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    GsClearOt(0, 0, &g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_gfx_clear_vram_and_scratch(0);
    GsSwapDispBuff();
    SetDispMask(0);
}
