#include "fft/battle.h"
#include "fft/battle_menu_status_panel.h"
#include "psx/types.h"

/* Select the menu-primitive palette bank from the panel style
 * or the global event mode. The palette
 * initializer receives `primitives` unchanged in $a0. */
void bunit_gfx_apply_menu_palette_for_mode(
    world_menu_palette_primitives_t* primitives, const battle_menu_status_panel_frame_config_t* thread) {
    if (thread->style == 1 || g_event_mode == 1) {
        battle_menu_init_primitive_colors_palette_bank_1(primitives);
    } else {
        battle_menu_init_primitive_colors_palette_bank_0(primitives);
    }
}
