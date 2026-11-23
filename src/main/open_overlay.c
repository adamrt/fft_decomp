/* Resident loaders for the title sequence and ending in OPEN.BIN. */

#include "fft/front/ending.h"
#include "fft/front/title.h"
#include "fft/sys/file.h"
#include "fft/sys/memory.h"
#include "fft/sys/overlay.h"

/* Load OPEN.BIN into the low overlay area and run its main loop; nonzero skip_movie skips the opening movie.
 * Returns OPEN.BIN's result, 0 meaning go straight into BATTLE.BIN. */
title_result_e overlay_run_title(s32 skip_movie) {
    file_load_into_buffer_checked(DISC_SECTOR_OPEN_OPEN_BIN, 0x36800, g_heap_low_overlay_load_address);
    return open_title_entrypoint(skip_movie);
}

/* Load OPEN.BIN into the low overlay area and play its ending cutscene. */
void overlay_run_ending(void) {
    file_load_into_buffer_checked(DISC_SECTOR_OPEN_OPEN_BIN, 0x36800, g_heap_low_overlay_load_address);
    open_ending_entrypoint();
}
