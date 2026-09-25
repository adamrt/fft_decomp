#include "fft/wldcore.h"

/* World render/present pass with the 3D map, plus the menu-window handoff.
 *
 * With `capture` == 0 it only drops the 0x10 visibility bit on the two active
 * window records and restores the saved system flags; otherwise it renders a
 * whole frame into both ordering tables, stores the composed image into the
 * scratch buffer, and re-arms the menu windows behind flag mask 0x3c20. */
void wldcore_gfx_toggle_captured_world_frame(s32 capture) {
    RECT display_rect;
    s32 buffer_index;

    if (capture == 0) {
        s32 map_window_index;
        s32 menu_window_index;

        map_window_index = g_wldcore_map_window_index;
        menu_window_index = g_wldcore_menu_stack_types[0];
        g_wldcore_window_records[map_window_index].flags &= ~0x10;
        g_main_system_flags = g_wldcore_saved_system_flags | 2;
        g_wldcore_window_records[menu_window_index].flags &= ~0x10;
        return;
    }

    DrawSync(0);
    VSync(0);
    world_gs_swapdispbuff();
    buffer_index = world_gs_get_active_buffer();
    g_active_graphics_buffer_index = buffer_index;
    world_gs_setworkbase(&g_wldcore_gfx_world_primitive_buffers + buffer_index * 0xE000);
    world_gs_clear_ot(0, 0, &g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    world_gs_clear_ot(0, 0, &g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_switch_to_stack((void*)0x1F8003FC);
    wldcore_map_project_and_cull_tiles();
    wldcore_gfx_draw_projected_map_tiles((s32)&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_map_draw_visible_routes(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_map_project_and_cull_dots();
    wldcore_map_update_camera_direction_and_projection();
    wldcore_gfx_draw_context_value_display(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_gfx_draw_calendar_date(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_proposition_submit_marker_request(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_dispatch_display_object_list(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index],
        (wldcore_display_object_t**)g_wldcore_window_aux_render_object_queue, g_wldcore_window_aux_render_object_count);
    wldcore_dispatch_display_object_list(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index],
        (wldcore_display_object_t**)g_wldcore_window_render_object_queue, g_wldcore_window_render_object_count);
    wldcore_restore_previous_stack();
    world_gs_sortclear(0, 0, 0, &g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    world_gs_draw_ot(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    world_gs_draw_ot(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    DrawSync(0);
    wldcore_gfx_set_display_rect(g_active_graphics_buffer_index, &display_rect.x);
    StoreImage(&display_rect, (u32*)g_wldcore_scratch_buffer);
    DrawSync(0);
    g_wldcore_loaded_background_set = -1;
    world_gs_clear_ot(0, 0, &g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_menu_run_world_frame(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    {
        s32 map_window_index;
        s32 menu_window_index;
        u32 saved_system_flags;

        saved_system_flags = g_main_system_flags;
        menu_window_index = g_wldcore_menu_stack_types[0];
        map_window_index = g_wldcore_map_window_index;
        g_wldcore_saved_system_flags = saved_system_flags;
        g_wldcore_window_records[map_window_index].flags |= 0x10;
        g_main_system_flags = saved_system_flags | 0x3C20;
        g_wldcore_window_records[menu_window_index].flags |= 0x10;
        wldcore_dispatch_display_object_list(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index],
            (wldcore_display_object_t**)g_wldcore_window_render_object_queue, g_wldcore_window_render_object_count);
    }
    world_gs_draw_ot(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    DrawSync(0);
}
