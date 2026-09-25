#include "fft/event_bunit.h"
#include "psx/types.h"

u8* bunit_cmd_set_semitrans_handler(u8* data) {
    g_bunit_gfx_semitrans_enabled = data[3];
    return data + data[1];
}
