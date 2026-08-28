#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

/* Copies two 0x20-byte palette rows into the menu palette color table. */
void battle_menu_copy_palette_colors(const void* source) {
    u8* destination;

    destination = g_battle_menu_palette_colors;
    battle_copy_bytes(destination, source, 0x20);
    battle_copy_bytes(destination + 0x20, (const u8*)source + 0x20, 0x20);
}
