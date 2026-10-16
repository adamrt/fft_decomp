#include "fft/gfx.h"
#include "fft/menu.h"

/* Return the menu text color. */
rgb24_t* menu_gfx_get_draw_color_variant(void) {
    return &g_menu_text_color;
}
