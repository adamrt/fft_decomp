#include "fft/card.h"
#include "psx/types.h"

s32 card_gfx_update_increasing_fade(void) {
    volatile u32 stack_padding[4];
    s32 result = 1;
    s32 speed = card_menu_get_event_speed();

    if (g_card_gfx_increasing_fade_active != 0) {
        g_card_gfx_fade_intensity += speed * 8;
        if (g_card_gfx_fade_intensity >= 0x100) {
            result = 0;
            g_card_gfx_increasing_fade_active = 0;
            g_card_gfx_fade_intensity = 0xff;
        }
        card_gfx_set_draw_mode_and_link_ot(0, 0, GetTPage(0, 2, 0x100, 0) & 0xffff, 0, g_card_gfx_otag_length - 2);
        g_card_gfx_fade_color_b = g_card_gfx_fade_intensity;
        g_card_gfx_fade_color_g = g_card_gfx_fade_intensity;
        g_card_gfx_fade_color[0] = g_card_gfx_fade_intensity;
        card_gfx_enqueue_translucent_tile(&g_card_gfx_fade_rect, g_card_gfx_fade_color, 1, g_card_gfx_otag_length - 1);
    }
    return result;
}
