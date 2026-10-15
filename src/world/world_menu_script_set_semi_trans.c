#include "fft/world.h"

/* Menu-script opcode handler: byte 3 of the instruction selects the
   semi-transparency flag used by the window/text primitives. Returns the
   address of the next instruction (byte 1 is the instruction length). */
menu_instruction_t* world_menu_script_set_semi_trans(menu_instruction_t* instruction) {
    g_menu_gfx_semitransparency = instruction->draw_state.value;
    return menu_next_instruction(instruction);
}
