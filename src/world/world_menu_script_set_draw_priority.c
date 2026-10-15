#include "fft/world.h"

/* Menu-script opcode handler: byte 3 of the instruction sets the menu draw
   priority. Returns the address of the next instruction. */
menu_instruction_t* world_menu_script_set_draw_priority(menu_instruction_t* instruction) {
    g_menu_gfx_otag_index = instruction->draw_state.value;
    return menu_next_instruction(instruction);
}
