/* Setting the menu text color. */

#include "fft/gfx.h"
#include "fft/menu.h"

/* Set the menu text color from an RGB triple. Unlike BUNIT's body there is no null
 * check; nothing in CARD.OUT or EQUIP.OUT calls this copy. */
void gfx_set_draw_color_variant(const rgb24_t* color) {
    g_menu_text_color.r = color->r;
    g_menu_text_color.g = color->g;
    g_menu_text_color.b = color->b;
}
