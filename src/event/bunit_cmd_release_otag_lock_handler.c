#include "fft/bunit.h"
#include "psx/types.h"

/* Twin of jobstts_cmd_release_otag_lock_handler (BUNIT does not
 * advance the OT index here). */
u8* bunit_cmd_release_otag_lock_handler(u8* data) {
    if (g_bunit_gfx_otag_index_locked != 0) {
        g_bunit_gfx_draw_area.y = 0;
        bunit_gfx_enqueue_draw_area(&g_bunit_gfx_draw_area, g_bunit_gfx_otag_index + 1);
        g_bunit_gfx_otag_index_locked = 0;
    }
    return data + data[1];
}
