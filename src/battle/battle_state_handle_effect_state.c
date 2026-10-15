#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Effect game-state handler: animate the units, the screen color modulation and
 * the camera while an effect plays. When the effect cycle has ended, the game
 * state and animation speed saved by battle_state_enter_effect_playback are
 * restored. Neither function that stores this state has a caller, so the
 * handler does not run. */
void battle_state_handle_effect_state(void) {
    battle_gfx_update_all_unit_rotation_and_vectors();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
        battle_gfx_update_screen_color_modulation_fade();
        camera_update_real_coord_animation();
        camera_update_screen_offset_animation();
        camera_update_zoom_animation();
        camera_update_rotation_animation();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    battle_gfx_draw_screen_color_modulation_overlay();
    if (g_battle_state_animation_continue_check == 0) {
        g_battle_game_state = g_battle_previous_game_state;
        g_main_system_animation_speed = g_main_saved_animation_speed;
    }
}
