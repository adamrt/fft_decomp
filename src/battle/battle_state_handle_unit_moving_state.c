#include "fft/battle.h"

void battle_state_handle_unit_moving_state(void) {
    battle_unit_misc_data_t* casting_misc_data;

    battle_camera_handle_rotation_input();
    battle_camera_call_zoom_map();
    battle_camera_call_toggle_tilt();
    casting_misc_data = battle_unit_get_casting_misc_data();
    battle_move_update_path_step(casting_misc_data);
    if (casting_misc_data->centre_tile_offset == 0
        && casting_misc_data->movement_path_offset >= casting_misc_data->movement_path_count
        && g_battle_state_animation_continue_check == 0) {
        battle_unit_update_display_by_misc_id(casting_misc_data->unit_id);
        battle_menu_open_move_confirm();
    }
    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
}
