#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_gfx.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

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
