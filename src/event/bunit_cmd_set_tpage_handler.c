#include "fft/event_bunit.h"

/* BUNIT.OUT 001c95c8 - Compute the TPage id from a stream command and cache it,
 * returning the next command pointer. */
const u8* bunit_cmd_set_tpage_handler(const u8* data) {
    g_bunit_gfx_texture_page = GetTPage(data[4], data[2] >> 4, data[3] << 4, data[2] << 8);
    return data + data[1];
}
