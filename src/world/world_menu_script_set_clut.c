#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Menu-script opcode handler: pick the CLUT x/y operands from bytes 2..4 or,
 * when g_world_menu_use_alternate_palette selects the alternate palette, bytes 5..7, and record the
 * resulting CLUT id. Advances by the instruction length (byte 1), like
 * world_menu_script_set_texture_page. */
u8* world_menu_script_set_clut(u8* op) {
    if (g_world_menu_use_alternate_palette != 0) {
        g_world_menu_clut = GetClut(op[6] << 4, op[7] + (op[5] << 8));
    } else {
        g_world_menu_clut = GetClut(op[3] << 4, op[4] + (op[2] << 8));
    }
    return op + op[1];
}
