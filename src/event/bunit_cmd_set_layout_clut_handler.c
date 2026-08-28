#include "fft/bunit.h"
#include "psx/types.h"

/* Set the sprite CLUT from the command's normal or compact-layout coordinates
 * and return the next command. */
u8* bunit_cmd_set_layout_clut_handler(u8* command) {
    if (g_bunit_text_layout_mode != 0) {
        g_bunit_gfx_clut_id = GetClut(command[6] << 4, command[7] | (command[5] << 8));
    } else {
        g_bunit_gfx_clut_id = GetClut(command[3] << 4, command[4] | (command[2] << 8));
    }
    return command + command[1];
}
