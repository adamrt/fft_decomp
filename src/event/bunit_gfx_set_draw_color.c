#include "fft/bunit.h"
#include "psx/types.h"

void bunit_gfx_set_draw_color(const u8* color) {
    if (color != 0) {
        g_bunit_gfx_sprite_color[0] = color[0];
        g_bunit_gfx_sprite_color[1] = color[1];
        g_bunit_gfx_sprite_color[2] = color[2];
    }
}
