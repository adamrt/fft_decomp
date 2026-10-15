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

/* Advances the live-map event state and submits its active screen fade.
 *
 * Keeping the loaded intensity and the clamped display level as separate
 * values retains the target's two moves before its unsigned 0x100 comparison.
 * The otherwise-unused 24-byte local preserves the original 64-byte frame. */
void battle_state_update_event_mode(void) {
    s16 intensity;
    u16 level;
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
    case 0:
        battle_gfx_init_evtchr_vram_slots();
        g_battle_menu_status_enabled = 1;
        ai_init_workspace();
        action_check_between_turn_events();
        break;
    case 9:
        battle_gfx_init_evtchr_vram_slots();
        battle_state_halve_animation_speed_and_queue_close(8, MAIN_GAME_FLOW_WORLD);
        break;
    case 10:
        battle_gfx_init_evtchr_vram_slots();
        battle_state_halve_animation_speed_and_queue_close(8, MAIN_GAME_FLOW_BATTLE);
        break;
    case 11:
        battle_gfx_init_evtchr_vram_slots();
        battle_state_halve_animation_speed_and_queue_close(8, MAIN_GAME_FLOW_RESET);
        break;
    case 19:
        battle_gfx_init_evtchr_vram_slots();
        battle_state_halve_animation_speed_and_queue_close(8, MAIN_GAME_FLOW_ENDING);
        break;
    }

    if (g_battle_screen_fade_intensity != 0) {
        intensity = g_battle_screen_fade_intensity;
        level = intensity;
        if (level >= 0x100) {
            level = 0xff;
        }
        g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = level;
        g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = level;
        g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = level;

        AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
        AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
    }
}
