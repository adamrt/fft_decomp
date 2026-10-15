#include "fft/event_bunit.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Step the fade to full intensity by 8 per event-speed step and draw the fade
 * tile over the screen. Returns 0 on the call that reaches 0xff and ends the
 * fade, 1 otherwise (also when no fade is active). */
s32 bunit_gfx_update_increasing_fade(void) {
    u8 unused[16];
    s32 ret = 1;
    s32 speed = menu_get_event_speed();

    if (g_bunit_gfx_increasing_fade_active != 0) {
        g_bunit_gfx_fade_intensity += speed * 8;
        if (g_bunit_gfx_fade_intensity >= 0x100) {
            g_bunit_gfx_increasing_fade_active = 0;
            g_bunit_gfx_fade_intensity = 0xFF;
            ret = 0;
        }
        menu_gfx_enqueue_draw_mode(0, 0, GetTPage(0, 2, 0x100, 0), 0, g_bunit_gfx_otag_count - 2);
        {
            rgb24_t* color = &g_bunit_gfx_fade_color;
            g_bunit_gfx_fade_color.b = g_bunit_gfx_fade_intensity;
            g_bunit_gfx_fade_color.g = g_bunit_gfx_fade_intensity;
            color->r = g_bunit_gfx_fade_intensity;
            gfx_enqueue_tile(g_bunit_gfx_fade_rect, color, 1, g_bunit_gfx_otag_count - 1);
        }
    }
    return ret;
}
