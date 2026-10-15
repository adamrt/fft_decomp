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

/* MapJumpingIn game-state handler: lighten the screen by
 * g_battle_state_screen_fade_step per frame. When it is clear, the saved game
 * state resumes at animation speed 1. */
void battle_state_handle_map_jumping_in_state(void) {
    u16 intensity;
    s32 previous_state;
    char unused[24];

    battle_state_handle_free_cursor_input();
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
        previous_state = g_battle_previous_game_state;
        g_battle_screen_fade_intensity = 0;
        g_battle_previous_game_state = BATTLE_GAME_STATE_FREE_CURSOR;
        g_main_system_animation_speed = 1;
        g_battle_game_state = previous_state;
    }
}
