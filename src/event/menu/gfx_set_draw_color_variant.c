#include "fft/gfx.h"
#include "fft/menu.h"

void gfx_set_draw_color_variant(const rgb24_t* color) {
    g_menu_text_color.r = color->r;
    g_menu_text_color.g = color->g;
    g_menu_text_color.b = color->b;
}
