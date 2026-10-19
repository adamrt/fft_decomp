/* Orbonne caption display setup, frame presentation and fades. */

#include "fft/gfx.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* A second copy of gfx_set_display_draw in the original, for the Orbonne
 * caption fades. Set up double-buffered drawing at width x height without
 * restarting the GPU: the second buffer sits 240 lines below the first (a
 * 480-line mode uses one), both draw environments clear to the given color,
 * and the display is masked until the caller turns it on. image_rect is not
 * used. */
static void new_game_set_display_draw(int width, int height, int projection, u8 red, u8 green, u8 blue) {
    RECT image_rect = { 0, 0, 1024, 512 };
    int second_y = height == 480 ? 0 : 240;

    SetDispMask(0);
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(projection);

    SetDefDrawEnv(&g_gfx_draw_envs[0], 0, 0, width, height);
    SetDefDispEnv(&g_gfx_display_envs[0], 0, second_y, width, height);
    SetDefDrawEnv(&g_gfx_draw_envs[1], 0, second_y, width, height);
    SetDefDispEnv(&g_gfx_display_envs[1], 0, 0, width, height);

    g_gfx_draw_envs[0].dtd = g_gfx_draw_envs[1].dtd = 0;
    g_gfx_draw_envs[0].isbg = g_gfx_draw_envs[1].isbg = 1;
    setRGB0(&g_gfx_draw_envs[0], red, green, blue);
    setRGB0(&g_gfx_draw_envs[1], red, green, blue);

    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);
}

/* Present one frame for the Orbonne caption fades: swap the display buffers after VSync, draw otag
 * and flush the debug print stream. Returns VSync's result. */
static int new_game_present_caption_frame(u32* otag) {
    /* Unused local retains the retail stack frame. */
    RECT matching_stack_space;
    int elapsed;

    g_gfx_screen_polarity = g_gfx_screen_polarity == 0;
    DrawSync(0);
    elapsed = VSync(0);

    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);
    DrawOTag(otag);
    FntFlush(-1);
    return elapsed;
}

/* Fade in a 256x24 caption strip on a black 256x240 screen.
 *
 * The strip is the texture at VRAM (896, 256) with its CLUT at (896, 287), drawn
 * at y 116. A full-screen subtractive quad steps from 248 to 0 in 32 frames, each
 * presented with new_game_present_caption_frame. No function in the tree calls this
 * routine or new_game_fade_out_orbonne_caption. */
void new_game_fade_in_orbonne_caption(void) {
    POLY_F4 fade_polygons[2];
    DR_MODE draw_modes[2];
    POLY_FT4 title_polygons[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* current_otag;
    s16 intensity = 248;

    new_game_set_display_draw(256, 240, 512, 0, 0, 0);

    SetPolyF4(&fade_polygons[0]);
    setRGB0(&fade_polygons[0], 0, 0, 0);
    setXY4(&fade_polygons[0], 0, 0, 256, 0, 0, 256, 256, 256);
    SetSemiTrans(&fade_polygons[0], 1);
    fade_polygons[1] = fade_polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &image_rect);
    draw_modes[1] = draw_modes[0];

    setPolyFT4(&title_polygons[0]);
    setRGB0(&title_polygons[0], 128, 128, 128);
    setXY4(&title_polygons[0], 0, 116, 255, 116, 0, 140, 255, 140);
    setUV4(&title_polygons[0], 0, 0, 255, 0, 0, 24, 255, 24);
    SetSemiTrans(&title_polygons[0], 0);
    title_polygons[0].tpage = GetTPage(0, 0, 896, 256);
    title_polygons[0].clut = GetClut(896, 287);
    title_polygons[1] = title_polygons[0];

    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 256;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
    SetDispMask(1);

    while (intensity >= 0) {
        current_otag = otags[g_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&fade_polygons[g_gfx_screen_polarity], intensity, intensity, intensity);
        AddPrim(current_otag, &fade_polygons[g_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_gfx_screen_polarity]);
        AddPrim(current_otag, &title_polygons[g_gfx_screen_polarity]);
        new_game_present_caption_frame(current_otag);
        intensity -= 8;
    }

    DrawSync(0);
    DrawSync(0);
}

/* Fade out the caption strip new_game_fade_in_orbonne_caption shows: the
 * subtractive quad steps from 0 to 255 in 32 frames over the same strip, then
 * both display buffers are cleared. */
void new_game_fade_out_orbonne_caption(void) {
    POLY_F4 fade_polygons[2];
    DR_MODE draw_modes[2];
    POLY_FT4 title_polygons[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* current_otag;
    s16 intensity = 0;

    SetPolyF4(&fade_polygons[0]);
    setRGB0(&fade_polygons[0], 0, 0, 0);
    setXY4(&fade_polygons[0], 0, 0, 256, 0, 0, 256, 256, 256);
    SetSemiTrans(&fade_polygons[0], 1);
    fade_polygons[1] = fade_polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &image_rect);
    draw_modes[1] = draw_modes[0];

    setPolyFT4(&title_polygons[0]);
    setRGB0(&title_polygons[0], 128, 128, 128);
    setXY4(&title_polygons[0], 0, 116, 255, 116, 0, 140, 255, 140);
    setUV4(&title_polygons[0], 0, 0, 255, 0, 0, 24, 255, 24);
    SetSemiTrans(&title_polygons[0], 0);
    title_polygons[0].tpage = GetTPage(0, 0, 896, 256);
    title_polygons[0].clut = GetClut(896, 287);
    title_polygons[1] = title_polygons[0];

    while (intensity < 256) {
        if (intensity >= 248) {
            intensity = 255;
        }

        current_otag = otags[g_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&fade_polygons[g_gfx_screen_polarity], intensity, intensity, intensity);
        AddPrim(current_otag, &fade_polygons[g_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_gfx_screen_polarity]);
        AddPrim(current_otag, &title_polygons[g_gfx_screen_polarity]);
        new_game_present_caption_frame(current_otag);
        intensity += 8;
    }

    DrawSync(0);
    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 256;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
}
