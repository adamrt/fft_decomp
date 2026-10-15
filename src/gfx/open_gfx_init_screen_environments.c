#include "fft/open.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"

/* Build the two draw/display environment pairs for the current screen mode.
 *
 * The screen is 320x240 with OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN and 256x240
 * without it. Buffer 0 draws at y 0 and displays y 240, buffer 1 the reverse. The
 * drawing offset puts the origin at the center of the screen, the background
 * clear is off, and the overlay fade's rectangle covers the screen in those
 * coordinates. OPEN_SYSTEM_RUNTIME_FLAG_RGB24 selects the 24-bit display the
 * movies use. Only the 320-pixel branch sets dfe.
 *
 * With clear_first the display is blanked first, the VRAM under both buffers is
 * cleared (half as wide again in 24-bit mode) and the active buffer's
 * environments are applied; without it only the records are filled. */
void open_gfx_init_screen_environments(s32 clear_first) {
    RECT rect;
    s32 put_envs;
    s16 center_x;

    if (clear_first != 0) {
        SetDispMask(0);
    }
    if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN) {
        SetDefDrawEnv(&g_open_gfx_screen_environments[0].draw, 0, 0, 0x140, 0xf0);
        SetDefDispEnv(&g_open_gfx_screen_environments[0].disp, 0, 0xf0, 0x140, 0xf0);
        SetDefDrawEnv(&g_open_gfx_screen_environments[1].draw, 0, 0xf0, 0x140, 0xf0);
        SetDefDispEnv(&g_open_gfx_screen_environments[1].disp, 0, 0, 0x140, 0xf0);
        center_x = 0xa0;
        g_open_gfx_screen_environments[0].draw.ofs[1] = 0x78;
        g_open_gfx_screen_environments[1].draw.ofs[1] = 0x168;
        g_open_gfx_screen_environments[0].draw.isbg = g_open_gfx_screen_environments[1].draw.isbg = 0;
        g_open_gfx_screen_environments[0].draw.dfe = g_open_gfx_screen_environments[1].draw.dfe = 1;
        g_open_gfx_overlay_fade.rect.x = -0xa0;
        g_open_gfx_overlay_fade.rect.y = -0x78;
        g_open_gfx_screen_environments[0].draw.ofs[0] = center_x;
        g_open_gfx_screen_environments[1].draw.ofs[0] = center_x;
        g_open_gfx_overlay_fade.rect.w = 0x140;
        g_open_gfx_overlay_fade.rect.h = 0xf0;
        if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_RGB24) {
            g_open_gfx_screen_environments[0].disp.isrgb24 = g_open_gfx_screen_environments[1].disp.isrgb24 = 1;
            rect.x = 0;
            rect.y = 0;
            rect.w = 0x1e0;
            rect.h = 0x1e0;
        } else {
            g_open_gfx_screen_environments[0].disp.isrgb24 = g_open_gfx_screen_environments[1].disp.isrgb24 = 0;
            rect.x = 0;
            rect.y = 0;
            rect.w = 0x140;
            rect.h = 0x1e0;
        }
    } else {
        SetDefDrawEnv(&g_open_gfx_screen_environments[0].draw, 0, 0, 0x100, 0xf0);
        SetDefDispEnv(&g_open_gfx_screen_environments[0].disp, 0, 0xf0, 0x100, 0xf0);
        SetDefDrawEnv(&g_open_gfx_screen_environments[1].draw, 0, 0xf0, 0x100, 0xf0);
        SetDefDispEnv(&g_open_gfx_screen_environments[1].disp, 0, 0, 0x100, 0xf0);
        center_x = 0x80;
        g_open_gfx_screen_environments[0].draw.ofs[1] = 0x78;
        g_open_gfx_screen_environments[1].draw.ofs[1] = 0x168;
        g_open_gfx_screen_environments[0].draw.isbg = g_open_gfx_screen_environments[1].draw.isbg = 0;
        g_open_gfx_overlay_fade.rect.x = -0x80;
        g_open_gfx_overlay_fade.rect.y = -0x78;
        g_open_gfx_screen_environments[0].draw.ofs[0] = center_x;
        g_open_gfx_screen_environments[1].draw.ofs[0] = center_x;
        g_open_gfx_overlay_fade.rect.w = 0x100;
        g_open_gfx_overlay_fade.rect.h = 0xf0;
        if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_RGB24) {
            g_open_gfx_screen_environments[0].disp.isrgb24 = g_open_gfx_screen_environments[1].disp.isrgb24 = 1;
            rect.x = 0;
            rect.y = 0;
            rect.w = 0x180;
            rect.h = 0x1e0;
        } else {
            g_open_gfx_screen_environments[0].disp.isrgb24 = g_open_gfx_screen_environments[1].disp.isrgb24 = 0;
            rect.x = 0;
            rect.y = 0;
            rect.w = 0x100;
            rect.h = 0x1e0;
        }
    }
    if (clear_first != 0) {
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
    }
    put_envs = clear_first;
    if (put_envs != 0) {
        PutDrawEnv(&g_open_gfx_screen_environments[g_active_graphics_buffer_index].draw);
        PutDispEnv(&g_open_gfx_screen_environments[g_active_graphics_buffer_index].disp);
        SetGeomOffset(0, 0);
    }
}
