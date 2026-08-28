#include "fft/world.h"
#include "psx/types.h"

/* Menu-script opcode handler: unless the flag is set, clears the y of
 * g_world_gfx_temporary_draw_area and re-registers the draw area at draw
 * priority + 1. Returns the address of the next instruction. */
u8* world_menu_script_reregister_temporary_draw_area(u8* script) {
    if (g_world_menu_temporary_draw_area_active == 0) {
        g_world_gfx_temporary_draw_area.y = 0;
        world_gfx_add_draw_area_primitive(&g_world_gfx_temporary_draw_area, g_world_menu_draw_priority + 1);
    }
    return script + script[1];
}
