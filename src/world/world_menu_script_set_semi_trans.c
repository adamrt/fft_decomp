#include "fft/world.h"
#include "psx/types.h"

/* Menu-script opcode handler: byte 3 of the instruction selects the
   semi-transparency flag used by the window/text primitives. Returns the
   address of the next instruction (byte 1 is the instruction length). */
u8* world_menu_script_set_semi_trans(u8* script) {
    g_world_menu_semi_trans = script[3];
    return script + script[1];
}
