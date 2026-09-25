#include "fft/event_bunit.h"
#include "psx/gpu.h"

/* BUNIT.OUT 001c9500 - Compute the CLUT id from a 5-byte cmd (x=data[3]<<4,
 * y=data[4]|(data[2]<<8)) and cache it; return the next command pointer. */
const u8* bunit_cmd_set_clut_handler(const u8* data) {
    g_bunit_gfx_clut_id = GetClut(data[3] << 4, data[4] | (data[2] << 8));
    return data + data[1];
}
