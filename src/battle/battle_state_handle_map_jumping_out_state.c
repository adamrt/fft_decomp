#include "fft/battle.h"

void battle_state_handle_map_jumping_out_state(void) {
    u32 intensity;
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

    g_battle_screen_fade_intensity += *(u16*)&g_battle_state_map_transition_step;
    if ((s16)g_battle_screen_fade_intensity >= 0x102) {
        g_battle_screen_fade_intensity = 0xff;
        g_battle_game_state = BATTLE_GAME_STATE_MAP_INITIALIZE;
        g_battle_state_map_init_step = 0;
        battle_state_stop_map_animations();
        battle_unit_clear_misc_units();
    }
}
