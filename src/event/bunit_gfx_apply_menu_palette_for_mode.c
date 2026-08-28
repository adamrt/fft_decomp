#include "fft/battle.h"
#include "psx/types.h"

/* BUNIT.OUT 001c1650 - Select the menu-primitive palette bank from the calling
 * thread's mode word (thread[4]) or the global event mode. The palette
 * initializer receives `primitives` unchanged in $a0. */
void bunit_gfx_apply_menu_palette_for_mode(s32 primitives, s32* thread) {
    if (thread[4] == 1 || g_event_mode == 1) {
        battle_menu_init_primitive_colors_palette_bank_1((world_menu_palette_primitives_t*)primitives);
    } else {
        battle_menu_init_primitive_colors_palette_bank_0((world_menu_palette_primitives_t*)primitives);
    }
}
