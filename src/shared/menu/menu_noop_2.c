#include "fft/menu.h"
#include "psx/types.h"

/* Empty in BATTLE and WORLD; in BATTLE it sits between menu_build_sprite_page
 * and menu_display_projected_action_effect_variant. Nothing on the disc calls it or
 * points at it; the name is a neutral placeholder until a reference turns up. */
void menu_noop_2(void) {
}
