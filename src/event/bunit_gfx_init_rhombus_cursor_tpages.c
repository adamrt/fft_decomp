#include "fft/event_bunit.h"
#include "psx/types.h"

void bunit_gfx_init_rhombus_cursor_tpages(void) {
    s16 term = -1;
    s32 i = 7;
    do {
        g_bunit_cursor_trail_points[i].x = term;
        i--;
    } while (i >= 0);
    g_bunit_cursor_shadow_sprite.tpage = GetTPage(0, 2, 0x3C0, 0x100);
    g_bunit_cursor_sprite.tpage = GetTPage(0, 1, 0x3C0, 0x100);
}
