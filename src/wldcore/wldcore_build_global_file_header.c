#include "fft/main_file.h"
#include "fft/main_runtime.h"
#include "fft/wldcore.h"

/* Target 0x80068d40. */
void wldcore_build_global_file_header(void) {
    wldcore_wait_and_build_file_header(&g_main_file_cd_state, g_main_battle_bin_sector, 1, 0);
}
