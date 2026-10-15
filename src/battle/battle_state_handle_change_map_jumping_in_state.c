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

/* MapJumpingIn2 game-state handler, the fade-in that ends an event's map change:
 * it advances the simulation unless the status screen owns the frame, runs the
 * event script and then draws the subtractive fade overlay one step lighter. When
 * the fade reaches zero the saved game state is restored. */
void battle_state_handle_change_map_jumping_in_state(void) {
    u16 intensity;
    char unused[24];

    battle_gfx_update_all_unit_rotation_and_vectors();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
        battle_gfx_update_screen_color_modulation_fade();
        camera_update_real_coord_animation();
        camera_update_screen_offset_animation();
        camera_update_zoom_animation();
        camera_update_rotation_animation();
        move_update_all_unit_paths();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    battle_gfx_draw_screen_color_modulation_overlay();
    switch (script_run_event_frame_variant(main_gfx_get_otag(), g_input_current_buttons)) {
    case 1:
        break;
    case 0:
        action_check_between_turn_events();
        break;
    case 9:
        battle_state_halve_animation_speed_and_queue_close(8, MAIN_GAME_FLOW_BATTLE);
        break;
    case 10:
        battle_state_halve_animation_speed_and_queue_close(8, MAIN_GAME_FLOW_WORLD);
        break;
    }

    intensity = g_battle_screen_fade_intensity;
    if (intensity >= 0x100) {
        intensity = 0xff;
    }
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = intensity;

    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);

    g_battle_screen_fade_intensity -= g_battle_state_screen_fade_step;
    if (g_battle_screen_fade_intensity <= 0) {
        g_battle_screen_fade_intensity = 0;
        g_battle_game_state = g_battle_previous_game_state;
        g_battle_previous_game_state = BATTLE_GAME_STATE_FREE_CURSOR;
    }
}
