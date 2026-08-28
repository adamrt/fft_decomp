#include "fft/main_gfx.h"

void main_gfx_fade_in_orbonne_monastery(void) {
    POLY_F4 fade_polygons[2];
    DR_MODE draw_modes[2];
    POLY_FT4 title_polygons[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* current_otag;
    s16 intensity;

    intensity = 248;
    main_set_display_draw_new_game(256, 240, 512, 0, 0, 0);

    SetPolyF4(&fade_polygons[0]);
    fade_polygons[0].r0 = 0;
    fade_polygons[0].g0 = 0;
    fade_polygons[0].b0 = 0;
    fade_polygons[0].x0 = 0;
    fade_polygons[0].y0 = 0;
    fade_polygons[0].x1 = 256;
    fade_polygons[0].y1 = 0;
    fade_polygons[0].x2 = 0;
    fade_polygons[0].y2 = 256;
    fade_polygons[0].x3 = 256;
    fade_polygons[0].y3 = 256;
    SetSemiTrans(&fade_polygons[0], 1);
    fade_polygons[1] = fade_polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, 0x40, &image_rect);
    draw_modes[1] = draw_modes[0];

    ((P_TAG*)&title_polygons[0])->len = 9;
    title_polygons[0].code = 0x2c;
    title_polygons[0].r0 = 128;
    title_polygons[0].g0 = 128;
    title_polygons[0].b0 = 128;
    title_polygons[0].x0 = 0;
    title_polygons[0].y0 = 116;
    title_polygons[0].x1 = 255;
    title_polygons[0].y1 = 116;
    title_polygons[0].x2 = 0;
    title_polygons[0].y2 = 140;
    title_polygons[0].x3 = 255;
    title_polygons[0].y3 = 140;
    title_polygons[0].u0 = 0;
    title_polygons[0].v0 = 0;
    title_polygons[0].u1 = 255;
    title_polygons[0].v1 = 0;
    title_polygons[0].u2 = 0;
    title_polygons[0].v2 = 24;
    title_polygons[0].u3 = 255;
    title_polygons[0].v3 = 24;
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
        current_otag = otags[g_main_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        g_main_gfx_screen_polarity[fade_polygons].r0 = intensity;
        g_main_gfx_screen_polarity[fade_polygons].g0 = intensity;
        g_main_gfx_screen_polarity[fade_polygons].b0 = intensity;
        AddPrim(current_otag, &fade_polygons[g_main_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_main_gfx_screen_polarity]);
        AddPrim(current_otag, &title_polygons[g_main_gfx_screen_polarity]);
        main_gfx_swap_display_area(current_otag);
        intensity -= 8;
    }

    DrawSync(0);
    DrawSync(0);
}
