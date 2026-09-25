#include "fft/event_equip.h"
#include "psx/types.h"

/* Set the sprite CLUT from the command's normal or compact-layout coordinates
 * and return the next command. */
u8* equip_cmd_set_layout_clut_handler(u8* command) {
    if (g_equip_text_compact_layout != 0) {
        g_equip_gfx_clut_id = GetClut(command[6] << 4, command[7] | (command[5] << 8));
    } else {
        g_equip_gfx_clut_id = GetClut(command[3] << 4, command[4] | (command[2] << 8));
    }
    return command + command[1];
}
