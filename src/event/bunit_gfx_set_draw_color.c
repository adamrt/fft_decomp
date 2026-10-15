#include "fft/event_bunit.h"
#include "fft/menu.h"

void bunit_gfx_set_draw_color(const rgb24_t* color) {
    if (color != 0) {
        g_menu_text_color.r = color->r;
        g_menu_text_color.g = color->g;
        g_menu_text_color.b = color->b;
    }
}
