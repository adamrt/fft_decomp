
/* Initialize the WLDCORE subsystems before entering the world loop.
 * The matched range includes the jr delay slot at 0x80067cb0. */
void wldcore_init_subsystems(void) {
    SetDispMask(0);
    wldcore_init_world_map_state();
    wldcore_init_input_repeat_state();
    wldcore_load_scratch_data_blocking();
    wldcore_gfx_init_subsystem();
    wldcore_window_init_record_pools();
    wldcore_map_init_tile_atlas();
    wldcore_load_formation_unit_name_and_face();
    wldcore_map_color_and_draw_dots();
    wldcore_noop_8008d514();
    wldcore_map_init_location_marker_and_camera();
    wldcore_menu_reset_window_list_and_store_origin();
    wldcore_init_runtime_state();
    wldcore_menu_restore_level_on_world_entry();
}
