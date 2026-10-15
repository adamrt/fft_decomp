#include "fft/event_card.h"
#include "fft/menu.h"

void gfx_set_draw_color_variant(const rgb24_t* data) {
    g_menu_text_color.r = data->r;
    g_menu_text_color.g = data->g;
    g_menu_text_color.b = data->b;
}
