#include "fft/memory.h"
#include "fft/menu.h"

/* Copies the normal and the dimmed 16-color menu palette into the menu palette color table. */
void menu_set_palette_colors(const menu_palette_pair_t* source) {
    rgba5551_t* destination;

    destination = g_menu_palette_colors.colors;
    mem_copy_bytes_nonempty(destination, source->colors, sizeof(palette_t));
    mem_copy_bytes_nonempty(destination + PALETTE_COLOR_COUNT, source->colors + PALETTE_COLOR_COUNT, sizeof(palette_t));
}
