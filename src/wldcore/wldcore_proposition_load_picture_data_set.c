#include "fft/wldcore.h"

/* Load picture set `set` into its image buffer, skipping the transfer when it
 * is already resident. */
void wldcore_proposition_load_picture_data_set(s32 set) {
    if (set != g_wldcore_loaded_picture_set) {
        file_wait_for_pending_variant();
        g_wldcore_loaded_picture_set = set;
        file_wait_then_build_header(&g_main_file_cd_state,
            g_wldcore_picture_sector_offsets[set] + DISC_SECTOR_WORLD_WLDPIC_BIN,
            g_wldcore_picture_sector_offsets[set + 1] - g_wldcore_picture_sector_offsets[set],
            g_wldcore_picture_buffer);
    }
}
