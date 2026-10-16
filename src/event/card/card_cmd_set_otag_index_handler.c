#include "fft/menu.h"
#include "psx/types.h"

const u8* card_cmd_set_otag_index_handler(const u8* data) {
    g_menu_gfx_otag_index = data[3];
    return data + data[1];
}
