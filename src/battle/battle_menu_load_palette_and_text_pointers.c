#include "fft/battle.h"
#include "fft/battle_text.h"
#include "psx/types.h"

void battle_menu_load_palette_and_text_pointers(void) {
    battle_menu_copy_palette_colors(g_battle_menu_default_palette_colors);
    battle_text_init_menu_section_pointers();
}
