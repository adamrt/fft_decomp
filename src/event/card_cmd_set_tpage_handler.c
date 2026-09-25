#include "fft/event_card.h"

const u8* card_cmd_set_tpage_handler(const u8* data) {
    g_card_gfx_texture_page = GetTPage(data[4], 0, data[3] << 4, data[2] << 8);
    return data + data[1];
}
