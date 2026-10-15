#include "fft/main.h"

/* Fade out the caption strip main_gfx_fade_in_orbonne_monastery shows: the
 * subtractive quad steps from 0 to 255 in 32 frames over the same strip, then
 * both display buffers are cleared. */
void main_gfx_fade_out_orbonne_monastery(void) {
    POLY_F4 fade_polygons[2];
    DR_MODE draw_modes[2];
    POLY_FT4 title_polygons[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* current_otag;
    s16 intensity;

    intensity = 0;

    SetPolyF4(&fade_polygons[0]);
    setRGB0(&fade_polygons[0], 0, 0, 0);
    setXY4(&fade_polygons[0], 0, 0, 256, 0, 0, 256, 256, 256);
    SetSemiTrans(&fade_polygons[0], 1);
    fade_polygons[1] = fade_polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, 0x40, &image_rect);
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

        current_otag = otags[g_main_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&g_main_gfx_screen_polarity[fade_polygons], intensity, intensity, intensity);
        AddPrim(current_otag, &fade_polygons[g_main_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_main_gfx_screen_polarity]);
        AddPrim(current_otag, &title_polygons[g_main_gfx_screen_polarity]);
        main_gfx_swap_display_area(current_otag);
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
