#include "fft/world.h"
#include "psx/types.h"

void world_menu_set_palette_colors(const u8* source) {
    world_script_copy_bytes(g_world_menu_palette_colors, source, 0x20);
    world_script_copy_bytes(g_world_menu_palette_colors + 0x20, source + 0x20, 0x20);
}
