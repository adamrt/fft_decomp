#include "fft/equip.h"
#include "psx/types.h"

const u8* equip_cmd_set_otag_index_handler(const u8* data) {
    g_equip_gfx_sprite_ot_index = data[3];
    return data + data[1];
}
