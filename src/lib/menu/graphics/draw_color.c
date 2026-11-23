/* The menu text color. */

#include "fft/gfx/gfx.h"
#include "fft/menu/menu_gfx.h"

/* Return the menu text color. */
rgb24_t* menu_gfx_get_draw_color(void) {
    return &g_menu_text_color;
}
