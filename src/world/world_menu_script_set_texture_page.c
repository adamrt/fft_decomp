#include "fft/world.h"
#include "psx/types.h"

/* Menu-script opcode handler: set the texture page from the operand bytes and
 * advance by the instruction length (byte 1), like world_menu_script_set_semi_trans. */
u8* world_menu_script_set_texture_page(u8* op) {
    g_world_menu_texture_page = GetTPage(op[4], op[2] >> 4, op[3] << 4, op[2] << 8);
    return op + op[1];
}
