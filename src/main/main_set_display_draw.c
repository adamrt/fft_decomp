#include "fft/main.h"
#include "psx/gpu.h"

void main_set_display_draw(int width, int height, int projection, u8 red, u8 green, u8 blue) {
    RECT image_rect = { 0, 0, 1024, 512 };
    int second_y = height == 480 ? 0 : 240;

    SetDispMask(0);
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(projection);

    SetDefDrawEnv(&g_main_gfx_draw_envs[0], 0, 0, width, height);
    SetDefDispEnv(&g_main_gfx_display_envs[0], 0, second_y, width, height);
    SetDefDrawEnv(&g_main_gfx_draw_envs[1], 0, second_y, width, height);
    SetDefDispEnv(&g_main_gfx_display_envs[1], 0, 0, width, height);

    g_main_gfx_draw_envs[0].dtd = g_main_gfx_draw_envs[1].dtd = 0;
    g_main_gfx_draw_envs[0].isbg = g_main_gfx_draw_envs[1].isbg = 1;
    g_main_gfx_draw_envs[0].r0 = red;
    g_main_gfx_draw_envs[0].g0 = green;
    g_main_gfx_draw_envs[0].b0 = blue;
    g_main_gfx_draw_envs[1].r0 = red;
    g_main_gfx_draw_envs[1].g0 = green;
    g_main_gfx_draw_envs[1].b0 = blue;

    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);
}
