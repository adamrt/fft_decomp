#include "fft/battle.h"
#include "psx/pad.h"

void battle_state_handle_display_move_area_state(void) {
    if (g_controller_input_pressed & PSX_PAD_CROSS) {
        battle_target_set_tile_background_color(0, 1);
        g_battle_controller_input = g_controller_input_copy_12;
        main_sound_play_sfx(MAIN_SFX_CANCEL);
        battle_state_set_free_cursor();
    }
    battle_camera_handle_rotation_input();
    battle_camera_call_zoom_map();
    battle_camera_call_toggle_tilt();
    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
}
