#include "fft/world.h"
#include "psx/libgpu.h"

/* Add a blend draw-mode primitive from menu-script opcode 0x18.
 *
 * Byte 2 selects GetTPage's blend rate. This function requires -O1 because
 * -O2 reschedules the prologue and fifth argument into a 100-byte function. */
menu_instruction_t* world_menu_script_add_blend_draw_mode(menu_instruction_t* instruction) {
    menu_gfx_enqueue_draw_mode_variant(
        0, 0, GetTPage(0, instruction->blend.blend_rate, 0x100, 0), 0, g_menu_gfx_otag_index);
    return menu_next_instruction(instruction);
}
