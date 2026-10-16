/* The soft reset of the game. */

#include "fft/main.h"
#include "fft/world.h"

/* Reset the game (soft reset). */
void world_game_reset(void) {
    main_system_reset_game();
}
