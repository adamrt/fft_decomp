#include "fft/menu.h"
#include "psx/types.h"

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * menu_finish_at_list_selection and menu_noop_6. Nothing on the disc calls it
 * or points at it; the name is a neutral placeholder until a reference turns
 * up. */
void menu_noop_5(void) {
}
