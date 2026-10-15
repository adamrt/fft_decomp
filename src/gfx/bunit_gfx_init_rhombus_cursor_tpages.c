#include "fft/event_bunit.h"
#include "psx/types.h"

/* Empty the eight cursor trail points (x = -1) and set the texture pages of
 * the cursor sprite and its shadow. */
void bunit_gfx_init_rhombus_cursor_tpages(void) {
    s16 term = -1;
    s32 i = 7;
    do {
        g_bunit_cursor_trail_points[i].x = term;
        i--;
    } while (i >= 0);
    g_bunit_cursor_shadow_sprite.source.tpage = GetTPage(0, 2, 0x3C0, 0x100);
    g_bunit_cursor_sprite.source.tpage = GetTPage(0, 1, 0x3C0, 0x100);
}
