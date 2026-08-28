#include "fft/world.h"

/* The caller at 0x800f28c4 supplies an ignored argument. */
void world_menu_init_palette_and_text(s32 unused) {
    world_menu_set_palette_colors(g_world_menu_default_palette_colors);
    world_text_init_section_pointers();
}
