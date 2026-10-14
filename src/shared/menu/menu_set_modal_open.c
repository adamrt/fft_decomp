#include "fft/battle_menu.h"

/* Sets the "an options/menu overlay is on screen" flag; the matching clear is
 * battle_menu_clear_modal_open. */
void menu_set_modal_open(void) {
    g_menu_modal_open = 1;
}
