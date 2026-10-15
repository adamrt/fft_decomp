#include "fft/wldcore.h"

/* Queue a one-sector load at BATTLE.BIN's sector with a null destination.
 * Called as the event transition level is pushed. */
/* Target 0x80068d40. */
void wldcore_file_seek_to_battle_bin(void) {
    file_wait_then_build_header(&g_main_file_cd_state, g_main_battle_bin_sector, 1, 0);
}
