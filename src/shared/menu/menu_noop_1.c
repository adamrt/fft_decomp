#include "fft/menu.h"
#include "psx/types.h"

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * text_configure_sprite_vram and menu_run_selected_tile_info_display_thread.
 * Nothing on the disc calls it or points at it; the name is a neutral
 * placeholder until a reference turns up. */
void menu_noop_1(void) {
}
