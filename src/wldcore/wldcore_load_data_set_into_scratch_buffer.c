#include "fft/wldcore.h"

/* Provisional: sector-offset table whose entry i is the start sector of set i
 * relative to the disc LBA below it, so entry i+1 minus entry i is that set's
 * sector count. Same shape as g_wldcore_sound_novel_script_sector_offsets and
 * g_wldcore_sound_novel_text_sector_offsets used by
 * wldcore_load_sound_novel_files. */

/* Load set `set` into the shared scratch buffer, skipping the transfer when it
 * is already resident. */
void wldcore_load_data_set_into_scratch_buffer(s32 set) {
    if (set != g_wldcore_loaded_background_set) {
        wldcore_wait_for_file_load();
        g_wldcore_loaded_background_set = set;
        wldcore_wait_and_build_file_header(&g_main_file_cd_state, g_wldcore_background_sector_offsets[set] + 0x12995,
            g_wldcore_background_sector_offsets[set + 1] - g_wldcore_background_sector_offsets[set],
            g_wldcore_scratch_buffer);
    }
}
