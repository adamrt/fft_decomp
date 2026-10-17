/* Setting the menu text color. */

#include "fft/gfx.h"
#include "fft/menu_gfx.h"

/* Set the menu text color. BUNIT accepts a null pointer; CARD and EQUIP do not check it. */
void gfx_set_draw_color(const rgb24_t* color) {
#ifdef FFT_EVENT_BUNIT
    /* CARD and EQUIP dereference the color without this guard. */
    if (color == 0) {
        return;
    }
#endif
    g_menu_text_color.r = color->r;
    g_menu_text_color.g = color->g;
    g_menu_text_color.b = color->b;
}
