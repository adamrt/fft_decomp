/* Resident GPU display setup and boot-logo presentation. Startup and session
 * resets use the same display buffers for the logos and shared resource loads;
 * BATTLE can replace the debug print font with EFC_FNT.TIM. */

#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/system.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Low immediate byte of FntLoad's CLUT-Y addiu instruction. */
extern u8 g_psyq_gpu_font_clut_y_offset_immediate;
extern u32 g_boot_squaresoft_logo_image_words[];

/* Install EFC_FNT.TIM as the debug print font: load the built-in font at VRAM (0x280, 0) with its
 * CLUT moved to row 0x7f, overwrite both with EFC_FNT.TIM (retrying until it reads) and open a
 * 512-character print stream at (0x88, 0x10) as the dump font. */
void debug_font_load(void) {
    TIM_IMAGE image;
    u32* data;
    int loaded = 0;

    g_psyq_gpu_font_clut_y_offset_immediate = 0x7f;
    FntLoad(0x280, 0);

    while (!loaded) {
        data = file_heap_load_checked(DISC_SECTOR_BATTLE_EFC_FNT_TIM, 0x8800);
        if (data != 0) {
            if (OpenTIM(data) == 0 && ReadTIM(&image) != 0) {
                LoadTPage(image.paddr, 0, 0, 0x280, 0, image.prect->w * 4, image.prect->h);
                loaded = 1;
                LoadClut2(image.caddr, 0x280, 0x7f);
            }
            heap_block_release(data);
        } else {
            VSync(0);
            file_load_poll(&g_file_cd_state);
        }
    }

    SetDumpFnt(FntOpen(0x88, 0x10, 0x100, 0x100, 0, 0x200));
    DrawSync(0);
    DrawSync(0);
}

/* Set up double-buffered drawing at width x height without restarting the GPU:
 * the second buffer sits 240 lines below the first (a 480-line mode uses one),
 * both draw environments clear to the given color, and the display is masked
 * until the caller turns it on. image_rect is not used. */
static void gfx_display_setup(int width, int height, int projection, u8 red, u8 green, u8 blue) {
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

/* Present one frame of the Squaresoft logo fade: swap the display buffers after VSync, upload the
 * 180x16 logo at (70, 112) of the new draw buffer and draw the caller's fade list over it. Returns
 * VSync's result. */
static int squaresoft_logo_draw(u32* otag, u32* image) {
    RECT destination;
    int elapsed;

    g_gfx_screen_polarity = g_gfx_screen_polarity == 0;
    DrawSync(0);
    elapsed = VSync(0);

    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);

    destination.x = 70;
    destination.y = g_gfx_draw_envs[g_gfx_screen_polarity].clip.y + 112;
    destination.w = 180;
    destination.h = 16;
    LoadImage(&destination, image);

    DrawOTag(otag);
    FntFlush(-1);
    return elapsed;
}

/* Fade in the Squaresoft logo from the image linked into the executable
 * (g_boot_squaresoft_logo_image_words).
 *
 * A full-screen subtractive quad steps from 248 to 0 in 32 frames while
 * squaresoft_logo_draw uploads the logo each frame. The logo stays on
 * screen while the caller loads its files; boot_squaresoft_logo_fade_out
 * removes it. */
void boot_squaresoft_logo_fade_in(void) {
    POLY_F4 polygons[2];
    DR_MODE draw_modes[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    s16 intensity = 248;
    int frame;
    int buffer;

    gfx_display_setup(320, 240, 512, 0, 0, 0);

    SetPolyF4(&polygons[0]);
    setRGB0(&polygons[0], 0, 0, 0);
    setXY4(&polygons[0], 0, 0, 320, 0, 0, 256, 320, 256);
    SetSemiTrans(&polygons[0], 1);
    polygons[1] = polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &image_rect);
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
        buffer = g_gfx_screen_polarity;
        ClearOTag(otags[buffer], 2);
        /* Reversed indexing preserves the retail compiler's addu operand order. */
        setRGB0(&g_gfx_screen_polarity[polygons], intensity, intensity, intensity);
        AddPrim(otags[buffer], &polygons[g_gfx_screen_polarity]);
        AddPrim(otags[buffer], &draw_modes[g_gfx_screen_polarity]);
        squaresoft_logo_draw(otags[buffer], g_boot_squaresoft_logo_image_words);
        intensity -= 8;
    }

    DrawSync(0);
    DrawSync(0);
}

/* Fade out the Squaresoft logo that boot_squaresoft_logo_fade_in left
 * on screen: the subtractive quad steps from 0 to 255 in 32 frames, then both
 * display buffers are cleared. */
void boot_squaresoft_logo_fade_out(void) {
    POLY_F4 polygons[2];
    DR_MODE draw_modes[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* otag_base;
    s16 intensity = 0;
    int buffer;

    SetPolyF4(&polygons[0]);
    setRGB0(&polygons[0], 0, 0, 0);
    setXY4(&polygons[0], 0, 0, 320, 0, 0, 256, 320, 256);
    SetSemiTrans(&polygons[0], 1);
    polygons[1] = polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &image_rect);
    draw_modes[1] = draw_modes[0];
    otag_base = otags[0];

    while (intensity < 256) {
        if (intensity >= 248) {
            intensity = 255;
        }

        buffer = g_gfx_screen_polarity;
        ClearOTag(otag_base + buffer * 2, 2);
        setRGB0(&g_gfx_screen_polarity[polygons], intensity, intensity, intensity);
        AddPrim(otag_base + buffer * 2, &polygons[g_gfx_screen_polarity]);
        AddPrim(otag_base + buffer * 2, &draw_modes[g_gfx_screen_polarity]);
        squaresoft_logo_draw(otag_base + buffer * 2, g_boot_squaresoft_logo_image_words);
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
static int sceap_logo_draw(u32* otag, u32* image) {
    RECT destination;
    int elapsed;

    g_gfx_screen_polarity = g_gfx_screen_polarity == 0;
    DrawSync(0);
    elapsed = VSync(0);

    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);

    destination.x = 0;
    destination.y = g_gfx_draw_envs[g_gfx_screen_polarity].clip.y + 104;
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
 * sceap_logo_draw. Both display buffers are cleared before and after. */
void boot_sceap_logo_show(void) {
    POLY_F4 polygons[2];
    DR_MODE draw_modes[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* current_otag;
    s16 intensity;
    int frame;

    gfx_display_setup(320, 240, 512, 0, 0, 0);
    file_buffer_load_checked(DISC_SECTOR_SCEAP_DAT, 0x5000, g_heap_high_overlay_load_address);
    frame = 0;

    SetPolyF4(&polygons[0]);
    setRGB0(&polygons[0], 0, 0, 0);
    setXY4(&polygons[0], 0, 0, 320, 0, 0, 240, 320, 240);
    SetSemiTrans(&polygons[0], 1);
    polygons[1] = polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &image_rect);
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
        current_otag = otags[g_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&g_gfx_screen_polarity[polygons], intensity, intensity, intensity);
        AddPrim(current_otag, &polygons[g_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_gfx_screen_polarity]);
        sceap_logo_draw(current_otag, (u32*)g_heap_high_overlay_load_address);
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

        current_otag = otags[g_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&g_gfx_screen_polarity[polygons], intensity, intensity, intensity);
        AddPrim(current_otag, &polygons[g_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_gfx_screen_polarity]);
        sceap_logo_draw(current_otag, (u32*)g_heap_high_overlay_load_address);
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

/* Restart the GPU and set up double-buffered drawing at width x height with the
 * second buffer 240 lines below the first (an interlaced 480-line mode uses one).
 *
 * Unlike gfx_display_setup this also calls ResetGraph(0) and InitGeom, clears
 * the whole 1024x512 VRAM, loads the debug font at (640, 0) and opens a print
 * stream, and leaves background clearing off. The display stays masked. */
void gfx_display_reset(int width, int height, int projection, u8 red, u8 green, u8 blue) {
    RECT image_rect = { 0, 0, 1024, 512 };
    int second_y = height == 480 ? 0 : 240;
    int font_stream;

    SetDispMask(0);
    ResetGraph(0);
    InitGeom();
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(projection);

    SetDefDrawEnv(&g_gfx_draw_envs[0], 0, 0, width, height);
    SetDefDispEnv(&g_gfx_display_envs[0], 0, second_y, width, height);
    SetDefDrawEnv(&g_gfx_draw_envs[1], 0, second_y, width, height);
    SetDefDispEnv(&g_gfx_display_envs[1], 0, 0, width, height);

    g_gfx_draw_envs[0].dtd = g_gfx_draw_envs[1].dtd = 0;
    g_gfx_draw_envs[0].isbg = g_gfx_draw_envs[1].isbg = 0;
    setRGB0(&g_gfx_draw_envs[0], red, green, blue);
    setRGB0(&g_gfx_draw_envs[1], red, green, blue);

    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
    FntLoad(640, 0);
    font_stream = FntOpen(40, 16, 512, 256, 0, 512);
    SetDumpFnt(font_stream);

    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);
}
