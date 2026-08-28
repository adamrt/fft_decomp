#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Menu script opcode: compute a CLUT id from the packed operands and record it.
 * Returns the next script instruction pointer.
 *
 * Built at -O1: the unscheduled prologue and load order are the -O1 signature
 * of the menu-script handlers (see world_menu_script_* in src/world). The
 * reassigned s32 y defeats plus->ior folding (gives addu). */
u8* world_menu_script_set_fixed_clut(u8* script) {
    s32 x;
    s32 y;

    x = script[3];
    x <<= 4;
    y = script[4];
    y += script[2] << 8;
    g_world_menu_clut = GetClut(x, y);
    return script + script[1];
}
