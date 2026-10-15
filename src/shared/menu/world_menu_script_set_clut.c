#include "fft/world.h"
#include "psx/libgpu.h"

/* Menu-script opcode handler: pick the CLUT x/y operands from bytes 2..4 or,
 * when g_world_menu_use_alternate_palette selects the alternate palette, bytes 5..7, and record the
 * resulting CLUT id. Advances by the instruction length (byte 1), like
 * world_menu_script_set_texture_page. */
menu_instruction_t* world_menu_script_set_clut(menu_instruction_t* instruction) {
    if (g_world_menu_use_alternate_palette != 0) {
        g_world_menu_clut = GetClut(instruction->clut.alternate_x_div_16 << 4,
            instruction->clut.alternate_y_low + (instruction->clut.alternate_y_high << 8));
    } else {
        g_world_menu_clut = GetClut(instruction->clut.normal_x_div_16 << 4,
            instruction->clut.normal_y_low + (instruction->clut.normal_y_high << 8));
    }
    return menu_next_instruction(instruction);
}
