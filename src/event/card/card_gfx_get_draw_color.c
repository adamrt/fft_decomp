#include "fft/event_card.h"
#include "fft/menu.h"

rgb24_t* card_gfx_get_draw_color(void) {
    return &g_menu_text_color;
}
