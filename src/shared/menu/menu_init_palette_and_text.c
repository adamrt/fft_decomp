#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/menu.h"

void menu_init_palette_and_text(s32 unused_0) {
    menu_set_palette_colors(&g_menu_default_palette_colors);
    text_init_section_pointers_variant();
}
