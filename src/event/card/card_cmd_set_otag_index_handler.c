#include "fft/menu.h"
#include "psx/types.h"

/* Menu script command: select the ordering-table slot later primitives go to (byte 3) and return
 * the next command (byte 1 holds this command's length). */
const u8* card_cmd_set_otag_index_handler(const u8* data) {
    g_menu_gfx_otag_index = data[3];
    return data + data[1];
}
