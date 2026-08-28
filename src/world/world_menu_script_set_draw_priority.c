#include "fft/world.h"
#include "psx/types.h"

/* Menu-script opcode handler: byte 3 of the instruction sets the menu draw
   priority. Returns the address of the next instruction. */
u8* world_menu_script_set_draw_priority(u8* script) {
    g_world_menu_draw_priority = script[3];
    return script + script[1];
}
