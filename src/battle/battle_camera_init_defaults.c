#include "fft/battle.h"
#include "psx/types.h"

void battle_camera_init_defaults(void) {
    main_util_set_vector(&g_battle_offset_screen_coords, 0x100, 0xA0, 0x280);
    main_util_set_vector(&g_battle_camera_zoom, ONE, ONE, ONE);
    main_util_set_svector(&g_battle_camera_render_state, 0x12E, 0xE00, 0);
    main_util_set_vector(&g_battle_camera_current_real_coords, 0, 0, 0);

    g_battle_casting_unit_id = 0xFF;
    g_battle_casting_misc_id = 0xFF;
    g_battle_cursor_y = 0;
    g_battle_cursor_z = 0;
    g_battle_cursor_x = 0;
    g_battle_camera_rotation_action = 0;
    g_battle_camera_zoom_action = 0;
    g_battle_map_zoom_target = 1;
    g_battle_camera_tilt_action = 0;
    g_battle_map_tilt_target = 1;
    g_battle_state_game_flow_running = 0;
}
