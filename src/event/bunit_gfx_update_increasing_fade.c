#include "fft/event_bunit.h"
#include "psx/types.h"

s32 bunit_gfx_update_increasing_fade(void) {
    u8 unused[16];
    s32 ret = 1;
    s32 speed = bunit_menu_get_event_speed();

    if (g_bunit_gfx_increasing_fade_active != 0) {
        g_bunit_gfx_fade_intensity += speed * 8;
        if (g_bunit_gfx_fade_intensity >= 0x100) {
            g_bunit_gfx_increasing_fade_active = 0;
            g_bunit_gfx_fade_intensity = 0xFF;
            ret = 0;
        }
        bunit_gfx_enqueue_draw_mode(0, 0, GetTPage(0, 2, 0x100, 0) & 0xFFFF, 0, g_bunit_gfx_otag_count - 2);
        {
            u8* color = g_bunit_gfx_fade_color;
            g_bunit_gfx_fade_color_b = g_bunit_gfx_fade_intensity;
            g_bunit_gfx_fade_color_g = g_bunit_gfx_fade_intensity;
            *color = g_bunit_gfx_fade_intensity;
            bunit_gfx_enqueue_translucent_tile(g_bunit_gfx_fade_rect, color, 1, g_bunit_gfx_otag_count - 1);
        }
    }
    return ret;
}
