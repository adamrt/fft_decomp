#include "fft/event_card.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Step the fade toward zero intensity by 8 per event-speed step and draw the
 * fade tile over the screen. Returns 0 on the call that reaches zero and
 * ends the fade, 1 otherwise (also when no fade is active). */
s32 gfx_update_decreasing_fade(void) {
    s32 result = 1;
    s32 speed = menu_get_event_speed();

    if (g_gfx_decreasing_fade_active != 0) {
        g_gfx_fade_intensity -= speed * 8;
        if (g_gfx_fade_intensity <= 0) {
            g_gfx_decreasing_fade_active = 0;
            result = 0;
        }
        menu_gfx_enqueue_draw_mode_variant(0, 0, GetTPage(0, 2, 0x100, 0), 0, g_gfx_otag_count - 2);
        {
            rgb24_t* color = &g_gfx_fade_color;

            g_gfx_fade_color.b = g_gfx_fade_intensity;
            g_gfx_fade_color.g = g_gfx_fade_intensity;
            color->r = g_gfx_fade_intensity;
            gfx_enqueue_tile(&g_gfx_fade_rect, color, 1, g_gfx_otag_count - 1);
        }
    }
    return result;
}
