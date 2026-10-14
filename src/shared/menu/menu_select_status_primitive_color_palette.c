#include "fft/battle_event.h"
#include "fft/battle_menu.h"
#include "fft/menu.h"

/* Select the menu primitives' normal or dimmed colors from the panel style
 * or the global event mode. The palette initializer receives
 * `primitives` unchanged in $a0. */
void menu_select_status_primitive_color_palette(
    menu_palette_primitives_t* primitives, const menu_status_thread_t* thread) {
    if (thread->style == 1 || g_menu_input_lock_mode == 1) {
        menu_init_primitive_colors_dimmed(primitives);
    } else {
        menu_init_primitive_colors_normal(primitives);
    }
}
