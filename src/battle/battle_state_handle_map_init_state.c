#include "fft/battle.h"
#include "fft/main_gfx.h"

void battle_state_handle_map_init_state(void) {
    char unused[24]; /* unreferenced; sizes the target's 0x30-byte frame */

    g_battle_state_map_init_step = battle_map_step_init_sequence(g_battle_map_id, g_battle_state_map_init_step);
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = 0xff;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = 0xff;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = 0xff;
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
    if (g_battle_state_map_init_step == 0xd) {
        g_battle_game_state = BATTLE_GAME_STATE_MAP_JUMPING_IN;
        g_battle_screen_fade_intensity = 0xff;
    }
}
