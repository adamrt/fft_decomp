#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Menu script opcode: compute a CLUT id from the packed operands and record it.
 * Returns the next script instruction pointer.
 *
 * Built at -O1: the unscheduled prologue and load order are the -O1 signature
 * of the menu-script handlers (see world_menu_script_* in src/world). The
 * reassigned s32 y defeats plus->ior folding (gives addu). */
menu_instruction_t* world_menu_script_set_fixed_clut(menu_instruction_t* instruction) {
    s32 x;
    s32 y;

    x = instruction->fixed_clut.x_div_16;
    x <<= 4;
    y = instruction->fixed_clut.y_low;
    y += instruction->fixed_clut.y_high << 8;
    g_world_menu_clut = GetClut(x, y);
    return menu_next_instruction(instruction);
}
