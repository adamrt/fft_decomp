/* Overlay screen fade state and steps. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

s32 gfx_get_fade_state(void) {
    s32 second = g_gfx_increasing_fade_active;
    s32 first = g_gfx_decreasing_fade_active;

    second *= 2;
    return first + second;
}

/* Start the fade from intensity 0xf0 down to zero (the screen fades in). */
void gfx_start_decreasing_fade(void) {
    g_gfx_decreasing_fade_active = 1;
    g_gfx_fade_intensity = 0xf0;
}

/* Start the fade from intensity 0 up to full (the screen fades out). */
void gfx_start_increasing_fade(void) {
    g_gfx_increasing_fade_active = 1;
    g_gfx_fade_intensity = 0;
}

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

/* Step the fade to full intensity by 8 per event-speed step and draw the fade
 * tile over the screen. Returns 0 on the call that reaches 0xff and ends the
 * fade, 1 otherwise (also when no fade is active). */
s32 gfx_update_increasing_fade(void) {
    u32 stack_padding[4];
    s32 result = 1;
    s32 speed = menu_get_event_speed();

    if (g_gfx_increasing_fade_active != 0) {
        g_gfx_fade_intensity += speed * 8;
        if (g_gfx_fade_intensity >= 0x100) {
            result = 0;
            g_gfx_increasing_fade_active = 0;
            g_gfx_fade_intensity = 0xff;
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
