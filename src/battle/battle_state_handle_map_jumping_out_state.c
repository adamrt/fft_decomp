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

/* MapJumpingOut game-state handler: darken the screen by
 * g_battle_state_screen_fade_step per frame. Past full black it stops the map
 * animations, clears the units and starts the map init sequence (MapInitialize),
 * which fades back in by itself. */
void battle_state_handle_map_jumping_out_state(void) {
    u16 intensity;
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

    g_battle_screen_fade_intensity += g_battle_state_screen_fade_step;
    if (g_battle_screen_fade_intensity >= 0x102) {
        g_battle_screen_fade_intensity = 0xff;
        g_battle_game_state = BATTLE_GAME_STATE_MAP_INITIALIZE;
        g_battle_state_map_init_step = 0;
        battle_state_stop_map_animations();
        battle_unit_clear_all();
    }
}
