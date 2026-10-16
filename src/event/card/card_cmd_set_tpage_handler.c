/* The menu command that sets the texture page. */

#include "fft/event_card.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Stream command: set the texture page from the command (color mode
 * data[4], x data[3] * 16, y data[2] * 256, blend rate 0) and return the
 * next command. */
const u8* card_cmd_set_tpage_handler(const u8* data) {
    g_card_gfx_texture_page = GetTPage(data[4], 0, data[3] << 4, data[2] << 8);
    return data + data[1];
}
