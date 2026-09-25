#include "fft/battle.h"

void battle_state_init_deployment_display(s32 width, s32 height, s32 projection, u8 red, u8 green, u8 blue) {
    RECT clear_rect;
    s32 second_y;

    clear_rect = g_deployment_clear_rect;
    second_y = -(height != 0x1e0) & 0xf0;

    SetDispMask(0);
    ResetGraph(1);
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(projection);

    SetDefDrawEnv(&g_main_gfx_draw_envs[0], -0x80, 0, width + 0x80, height);
    SetDefDispEnv(&g_main_gfx_display_envs[0], 0, second_y, width, height);
    SetDefDrawEnv(&g_main_gfx_draw_envs[1], -0x80, second_y, width + 0x80, height);
    SetDefDispEnv(&g_main_gfx_display_envs[1], 0, 0, width, height);

    g_main_gfx_draw_envs[0].dtd = g_main_gfx_draw_envs[1].dtd = 1;
    g_main_gfx_draw_envs[0].isbg = g_main_gfx_draw_envs[1].isbg = 0;
    g_main_gfx_draw_envs[0].r0 = red;
    g_main_gfx_draw_envs[0].g0 = green;
    g_main_gfx_draw_envs[0].b0 = blue;
    g_main_gfx_draw_envs[1].r0 = red;
    g_main_gfx_draw_envs[1].g0 = green;
    g_main_gfx_draw_envs[1].b0 = blue;

    ClearImage(&clear_rect, 0, 0, 0);
    DrawSync(0);
    if (g_font_print_enabled != 0) {
        main_gfx_load_efc_fnt();
    }
    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);
}
