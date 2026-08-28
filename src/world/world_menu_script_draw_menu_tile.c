#include "fft/world.h"
#include "psx/types.h"

/* Menu-script opcode handler: bytes 2..5 give x, y, w, h; the payload
 * starts at byte 6. Returns the address of the next instruction. */
u8* world_menu_script_draw_menu_tile(u8* script) {
    s16 rect[4];

    rect[0] = script[2];
    rect[1] = script[3];
    rect[2] = script[4];
    rect[3] = script[5];
    world_menu_add_tile_primitive(rect, script + 6, (u8)g_world_menu_semi_trans, g_world_menu_draw_priority);
    return script + script[1];
}
