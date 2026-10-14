#include "fft/battle_menu.h"

/* Clears the "an options/menu overlay is on screen" flag. Called from the
 * BATTLE.BIN overlay loaders (EQUIP.OUT, DEBUGCHR.OUT, ...) once the overlay
 * has been torn down. */
void menu_clear_modal_open(void) {
    g_menu_modal_open = 0;
}
