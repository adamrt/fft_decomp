#include "fft/main.h"
#include "psx/libgpu.h"

/* Restart the GPU and set up double-buffered drawing at width x height with the
 * second buffer 240 lines below the first (an interlaced 480-line mode uses one).
 *
 * Unlike main_set_display_draw this also calls ResetGraph(0) and InitGeom, clears
 * the whole 1024x512 VRAM, loads the debug font at (640, 0) and opens a print
 * stream, and leaves background clearing off. The display stays masked. */
void main_gfx_reset_display(int width, int height, int projection, u8 red, u8 green, u8 blue) {
    RECT image_rect = { 0, 0, 1024, 512 };
    int second_y = height == 480 ? 0 : 240;
    int font_stream;

    SetDispMask(0);
    ResetGraph(0);
    InitGeom();
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(projection);

    SetDefDrawEnv(&g_main_gfx_draw_envs[0], 0, 0, width, height);
    SetDefDispEnv(&g_main_gfx_display_envs[0], 0, second_y, width, height);
    SetDefDrawEnv(&g_main_gfx_draw_envs[1], 0, second_y, width, height);
    SetDefDispEnv(&g_main_gfx_display_envs[1], 0, 0, width, height);

    g_main_gfx_draw_envs[0].dtd = g_main_gfx_draw_envs[1].dtd = 0;
    g_main_gfx_draw_envs[0].isbg = g_main_gfx_draw_envs[1].isbg = 0;
    setRGB0(&g_main_gfx_draw_envs[0], red, green, blue);
    setRGB0(&g_main_gfx_draw_envs[1], red, green, blue);

    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
    FntLoad(640, 0);
    font_stream = FntOpen(40, 16, 512, 256, 0, 512);
    SetDumpFnt(font_stream);

    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);
}
