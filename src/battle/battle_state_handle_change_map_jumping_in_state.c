#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_move.h"
#include "fft/battle_state.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Per-frame update for a battle state that fades the screen out: it advances
 * the simulation unless the status screen owns the frame, runs the event
 * script and then draws the fade overlay one step darker. When the fade
 * reaches zero the previous game state is restored. */
void battle_state_handle_change_map_jumping_in_state(void) {
    u32 intensity;
    char unused[24];

    battle_gfx_update_all_unit_rotation_and_vectors();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
        battle_gfx_update_screen_color_modulation_fade();
        battle_camera_update_real_coord_animation();
        battle_camera_update_offset_screen_coord_animation();
        battle_camera_update_zoom_animation();
        battle_camera_update_rotation_animation();
        battle_move_update_unit_by_misc_id();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    battle_gfx_draw_screen_color_modulation_overlay();
    switch (battle_script_run_event_frame(main_gfx_get_otag(), g_controller_input_raw)) {
    case 1:
        break;
    case 0:
        battle_action_check_between_turn_events();
        break;
    case 9:
        battle_state_halve_animation_speed_and_queue_close(8, 0);
        break;
    case 10:
        battle_state_halve_animation_speed_and_queue_close(8, 1);
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

    g_battle_screen_fade_intensity -= *(u16*)&g_battle_state_map_transition_step;
    if ((s16)g_battle_screen_fade_intensity <= 0) {
        g_battle_screen_fade_intensity = 0;
        g_battle_game_state = g_previous_battle_game_state;
        g_previous_battle_game_state = 0;
    }
}
