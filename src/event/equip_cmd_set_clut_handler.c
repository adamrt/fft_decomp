#include "fft/event_equip.h"
#include "psx/gpu.h"

const u8* equip_cmd_set_clut_handler(const u8* data) {
    g_equip_gfx_clut_id = GetClut(data[3] << 4, data[4] | (data[2] << 8));
    return data + data[1];
}
