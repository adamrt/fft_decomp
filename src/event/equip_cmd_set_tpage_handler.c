#include "fft/equip.h"

const u8* equip_cmd_set_tpage_handler(const u8* data) {
    g_equip_gfx_texture_page = GetTPage(data[4], data[2] >> 4, data[3] << 4, data[2] << 8);
    return data + data[1];
}
