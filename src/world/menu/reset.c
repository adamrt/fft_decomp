/* The soft reset of the game. */

#include "fft/menu_screen.h"
#include "fft/system.h"

/* Reset the game (soft reset). */
void menu_screen_reset_game(void) {
    system_reset_game();
}
