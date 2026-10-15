/* The Squaresoft and SCE boot logos, their drawing and fade out. */

#include "fft/main.h"
#include "psx/libgpu.h"

int main_boot_draw_squaresoft_logo(u32* otag, u32* image) {
    RECT destination;
    int elapsed;

    g_main_gfx_screen_polarity = g_main_gfx_screen_polarity == 0;
    DrawSync(0);
    elapsed = VSync(0);

    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);

    destination.x = 70;
    destination.y = g_main_gfx_draw_envs[g_main_gfx_screen_polarity].clip.y + 112;
    destination.w = 180;
    destination.h = 16;
    LoadImage(&destination, image);

    DrawOTag(otag);
    FntFlush(-1);
    return elapsed;
}

/* Fade in the Squaresoft logo from the image linked into the executable
 * (g_main_boot_squaresoft_logo_image_words).
 *
 * A full-screen subtractive quad steps from 248 to 0 in 32 frames while
 * main_boot_draw_squaresoft_logo uploads the logo each frame. The logo stays on
 * screen while the caller loads its files; main_boot_fade_out_squaresoft_logo
 * removes it. */
void main_boot_build_and_draw_squaresoft_logo(void) {
    POLY_F4 polygons[2];
    DR_MODE draw_modes[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    s16 intensity;
    int frame;
    int buffer;

    intensity = 248;
    main_set_display_draw(320, 240, 512, 0, 0, 0);

    SetPolyF4(&polygons[0]);
    setRGB0(&polygons[0], 0, 0, 0);
    setXY4(&polygons[0], 0, 0, 320, 0, 0, 256, 320, 256);
    SetSemiTrans(&polygons[0], 1);
    polygons[1] = polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, 0x40, &image_rect);
    draw_modes[1] = draw_modes[0];

    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 256;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
    SetDispMask(1);

    for (frame = 0; frame < 15; frame++) {
        VSync(0);
    }

    while (intensity >= 0) {
        buffer = g_main_gfx_screen_polarity;
        ClearOTag(otags[buffer], 2);
        /* Reversed indexing preserves the retail compiler's addu operand order. */
        setRGB0(&g_main_gfx_screen_polarity[polygons], intensity, intensity, intensity);
        AddPrim(otags[buffer], &polygons[g_main_gfx_screen_polarity]);
        AddPrim(otags[buffer], &draw_modes[g_main_gfx_screen_polarity]);
        main_boot_draw_squaresoft_logo(otags[buffer], g_main_boot_squaresoft_logo_image_words);
        intensity -= 8;
    }

    DrawSync(0);
    DrawSync(0);
}

/* Fade out the Squaresoft logo that main_boot_build_and_draw_squaresoft_logo left
 * on screen: the subtractive quad steps from 0 to 255 in 32 frames, then both
 * display buffers are cleared. */
void main_boot_fade_out_squaresoft_logo(void) {
    POLY_F4 polygons[2];
    DR_MODE draw_modes[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* otag_base;
    s16 intensity;
    int buffer;

    intensity = 0;
    SetPolyF4(&polygons[0]);
    setRGB0(&polygons[0], 0, 0, 0);
    setXY4(&polygons[0], 0, 0, 320, 0, 0, 256, 320, 256);
    SetSemiTrans(&polygons[0], 1);
    polygons[1] = polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, 0x40, &image_rect);
    draw_modes[1] = draw_modes[0];
    otag_base = otags[0];

    while (intensity < 256) {
        if (intensity >= 248) {
            intensity = 255;
        }

        buffer = g_main_gfx_screen_polarity;
        ClearOTag(otag_base + buffer * 2, 2);
        setRGB0(&g_main_gfx_screen_polarity[polygons], intensity, intensity, intensity);
        AddPrim(otag_base + buffer * 2, &polygons[g_main_gfx_screen_polarity]);
        AddPrim(otag_base + buffer * 2, &draw_modes[g_main_gfx_screen_polarity]);
        main_boot_draw_squaresoft_logo(otag_base + buffer * 2, g_main_boot_squaresoft_logo_image_words);
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

/* Present one frame of the SCEAP.DAT fade: swap the display buffers after VSync,
 * upload the 320x32 strip at line 104 of the new draw buffer and draw the
 * caller's fade list over it. Returns VSync's result. */
int main_boot_draw_sceap_logo(u32* otag, u32* image) {
    RECT destination;
    int elapsed;

    g_main_gfx_screen_polarity = g_main_gfx_screen_polarity == 0;
    DrawSync(0);
    elapsed = VSync(0);

    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);

    destination.x = 0;
    destination.y = g_main_gfx_draw_envs[g_main_gfx_screen_polarity].clip.y + 104;
    destination.w = 320;
    destination.h = 32;
    LoadImage(&destination, image);

    DrawOTag(otag);
    FntFlush(-1);
    return elapsed;
}

/* Show the publisher's logo strip at power-on.
 *
 * Loads SCEAP.DAT (sector 198, a 320x32 16-bit image) to the high overlay address
 * and switches to a 320x240 display. A full-screen subtractive black quad fades
 * the strip in over 32 frames, it stays for 180 frames, and the same quad fades
 * it out over 32; each frame uploads the image again through
 * main_boot_draw_sceap_logo. Both display buffers are cleared before and after. */
void main_boot_build_and_draw_sceap_logo(void) {
    POLY_F4 polygons[2];
    DR_MODE draw_modes[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* current_otag;
    s16 intensity;
    int frame;

    main_set_display_draw(320, 240, 512, 0, 0, 0);
    main_file_load_checked_to_address(DISC_SECTOR_SCEAP_DAT, 0x5000, g_main_heap_high_overlay_load_address);
    frame = 0;

    SetPolyF4(&polygons[0]);
    setRGB0(&polygons[0], 0, 0, 0);
    setXY4(&polygons[0], 0, 0, 320, 0, 0, 240, 320, 240);
    SetSemiTrans(&polygons[0], 1);
    polygons[1] = polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, 0x40, &image_rect);
    draw_modes[1] = draw_modes[0];

    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 320;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
    SetDispMask(1);

    for (; frame < 15; frame++) {
        VSync(0);
    }

    intensity = 248;
    while (intensity >= 0) {
        current_otag = otags[g_main_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&g_main_gfx_screen_polarity[polygons], intensity, intensity, intensity);
        AddPrim(current_otag, &polygons[g_main_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_main_gfx_screen_polarity]);
        main_boot_draw_sceap_logo(current_otag, (u32*)g_main_heap_high_overlay_load_address);
        intensity -= 8;
    }

    for (frame = 0; frame < 180; frame++) {
        VSync(0);
    }

    intensity = 0;
    while (intensity < 256) {
        if (intensity >= 248) {
            intensity = 255;
        }

        current_otag = otags[g_main_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&g_main_gfx_screen_polarity[polygons], intensity, intensity, intensity);
        AddPrim(current_otag, &polygons[g_main_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_main_gfx_screen_polarity]);
        main_boot_draw_sceap_logo(current_otag, (u32*)g_main_heap_high_overlay_load_address);
        intensity += 8;
    }

    DrawSync(0);
    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 320;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
}
