#include "fft/battle.h"
#include "psx/types.h"

void battle_state_handle_effect_state(void) {
    battle_gfx_update_all_unit_rotation_and_vectors();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
        battle_gfx_update_screen_color_modulation_fade();
        battle_camera_update_real_coord_animation();
        battle_camera_update_offset_screen_coord_animation();
        battle_camera_update_zoom_animation();
        battle_camera_update_rotation_animation();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    battle_gfx_draw_screen_color_modulation_overlay();
    if (g_battle_state_animation_continue_check == 0) {
        g_battle_game_state = g_previous_battle_game_state;
        g_animation_speed = g_main_saved_animation_speed;
    }
}
