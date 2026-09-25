#include "fft/wldcore.h"

/* Load picture set `set` into its image buffer, skipping the transfer when it
 * is already resident. */
void wldcore_proposition_load_picture_data_set(s32 set) {
    if (set != g_wldcore_loaded_picture_set) {
        wldcore_wait_for_file_load();
        g_wldcore_loaded_picture_set = set;
        wldcore_wait_and_build_file_header(&g_main_file_cd_state, g_wldcore_picture_sector_offsets[set] + 0x11DAE,
            g_wldcore_picture_sector_offsets[set + 1] - g_wldcore_picture_sector_offsets[set],
            (void*)g_wldcore_picture_buffer);
    }
}
