#include "fft/event_bunit.h"
#include "psx/types.h"

/* BUNIT.OUT 001c9634 - Build a TPage id from data[2] and submit a sprite prim
 * to the current OT. Returns the next command pointer. */
u8* bunit_cmd_enqueue_draw_mode_handler(u8* data) {
    bunit_gfx_enqueue_draw_mode(0, 0, GetTPage(0, data[2], 0x100, 0) & 0xFFFF, 0, g_bunit_gfx_otag_index);
    return data + data[1];
}
