#include "fft/menu.h"
#include "psx/types.h"

/* Empty in BATTLE and WORLD; in BATTLE it sits between menu_noop_5 and
 * menu_start_unit_view_thread. Nothing on the disc calls it or points at it;
 * the name is a neutral placeholder until a reference turns up. */
void menu_noop_6(void) {
}
