#include "fft/battle_runtime.h"

/* Sets the "an options/menu overlay is on screen" flag; the matching clear is
 * battle_menu_clear_option_menu_open. */
void battle_menu_set_option_menu_open(void) {
    g_option_menu_open = 1;
}
