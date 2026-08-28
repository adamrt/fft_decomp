#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Add a blend draw-mode primitive from menu-script opcode 0x18.
 *
 * Byte 2 selects GetTPage's blend rate. This function requires -O1 because
 * -O2 reschedules the prologue and fifth argument into a 100-byte function. */
u8* world_menu_script_add_blend_draw_mode(u8* script) {
    world_gfx_add_draw_mode_primitive(0, 0, (u16)GetTPage(0, script[2], 0x100, 0), 0, g_world_menu_draw_priority);
    return script + script[1];
}
