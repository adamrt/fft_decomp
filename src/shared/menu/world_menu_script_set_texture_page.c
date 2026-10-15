#include "fft/world.h"

/* Menu-script opcode handler: set the texture page from the operand bytes and
 * advance by the instruction length (byte 1), like world_menu_script_set_semi_trans. */
menu_instruction_t* world_menu_script_set_texture_page(menu_instruction_t* instruction) {
    g_world_menu_texture_page = GetTPage(instruction->texture_page.depth, instruction->texture_page.y_blend >> 4,
        instruction->texture_page.x_div_16 << 4, instruction->texture_page.y_blend << 8);
    return menu_next_instruction(instruction);
}
