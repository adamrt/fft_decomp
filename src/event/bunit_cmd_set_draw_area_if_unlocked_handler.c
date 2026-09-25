#include "fft/event_bunit.h"
#include "psx/types.h"

/* Twin of jobstts_cmd_set_draw_area_if_unlocked_handler. */
u8* bunit_cmd_set_draw_area_if_unlocked_handler(u8* data) {
    RECT area;

    if (g_bunit_gfx_otag_index_locked == 0) {
        area.x = data[2];
        area.y = data[3];
        area.w = data[4];
        area.h = data[5];
        bunit_gfx_enqueue_draw_area(&area, g_bunit_gfx_otag_index - 1);
    }
    return data + data[1];
}
