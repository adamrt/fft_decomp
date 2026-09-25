#include "fft/event_equip.h"
#include "psx/types.h"

const u8* equip_cmd_set_semitrans_handler(const u8* data) {
    g_equip_gfx_semitransparency = data[3];
    return data + data[1];
}
