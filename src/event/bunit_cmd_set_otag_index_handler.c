#include "fft/event_bunit.h"
#include "psx/types.h"

u8* bunit_cmd_set_otag_index_handler(u8* data) {
    g_bunit_gfx_otag_index = data[3];
    return data + data[1];
}
