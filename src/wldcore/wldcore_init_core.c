#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Initialize the WLDCORE world buffers, menu state, and runtime tables. */
void wldcore_init_core(void) {
    RECT rect;
    s32 count;

    DrawSync(0);
    VSync(0);
    GsSwapDispBuff();
    count = GsGetActiveBuff();
    g_active_graphics_buffer_index = count;
    GsSetWorkBase(g_wldcore_gfx_world_primitive_buffers + count * 0xE000);
    GsClearOt(0, 0, &g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_draw_close_indicator_if_pending(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_switch_to_stack(getScratchAddr(255));
    wldcore_gfx_draw_projected_map_tiles(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_map_draw_visible_routes(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_map_project_and_cull_dots();
    window_dispatch_record_list_variant(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index],
        g_wldcore_window_aux_render_object_queue, g_wldcore_window_aux_render_object_count);
    wldcore_restore_previous_stack();
    GsSortClear(0, 0, 0, &g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    GsDrawOt(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    DrawSync(0);
    wldcore_gfx_set_display_rect(g_active_graphics_buffer_index, &rect);
    StoreImage(&rect, g_wldcore_scratch_buffer);
    DrawSync(0);
    g_wldcore_loaded_background_set = -1;
    GsClearOt(0, 0, &g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_map_update_camera_direction_and_projection();
    wldcore_menu_run_world_frame(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_gfx_draw_context_value_display(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_gfx_draw_calendar_date(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_proposition_submit_marker_request(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    window_dispatch_record_list_variant(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index],
        g_wldcore_window_render_object_queue, g_wldcore_window_render_object_count);
    GsDrawOt(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    DrawSync(0);
}
