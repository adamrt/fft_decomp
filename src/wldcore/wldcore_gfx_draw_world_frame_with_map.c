#include "fft/wldcore.h"

/* Per-frame WLDCORE render pass with the 3D map: clears both ordering
 * tables, projects and draws the map tiles into the auxiliary table and the
 * map dots and menus into the main one, then sorts the auxiliary table's
 * clear, draws both tables and clears system flag 2. Sibling of
 * wldcore_gfx_draw_world_frame. */
void wldcore_gfx_draw_world_frame_with_map(void) {
    world_gs_clear_ot(0, 0, &g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    world_gs_clear_ot(0, 0, &g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_draw_close_indicator_if_pending(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_switch_to_stack((void*)0x1F8003FC);
    wldcore_map_project_and_cull_tiles();
    wldcore_gfx_draw_projected_map_tiles((s32)&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_restore_previous_stack();
    wldcore_map_step_route_travel();
    wldcore_switch_to_stack((void*)0x1F8003FC);
    wldcore_map_draw_visible_routes(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_map_draw_path_animation(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_map_project_and_cull_dots();
    wldcore_map_update_camera_direction_and_projection();
    wldcore_restore_previous_stack();
    wldcore_menu_run_world_frame(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_switch_to_stack((void*)0x1F8003FC);
    wldcore_gfx_draw_context_value_display(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_gfx_draw_calendar_date(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_proposition_submit_marker_request(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_dispatch_display_object_list(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index],
        (wldcore_display_object_t**)g_wldcore_window_aux_render_object_queue, g_wldcore_window_aux_render_object_count);
    wldcore_dispatch_display_object_list(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index],
        (wldcore_display_object_t**)g_wldcore_window_render_object_queue, g_wldcore_window_render_object_count);
    wldcore_restore_previous_stack();
    DrawSync(0);
    wldcore_reset_game_if_special_keycode_is_pressed();
    VSync(0);
    world_gs_swapdispbuff();
    world_gs_sortclear(0, 0, 0, &g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    world_gs_draw_ot(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    world_gs_draw_ot(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    g_main_system_flags &= ~2;
}
