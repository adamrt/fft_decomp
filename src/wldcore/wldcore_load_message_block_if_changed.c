#include "fft/main_file.h"
#include "fft/wldcore.h"

/* Provisional: a seven-entry sector-offset table, the same shape as the pair
 * used by wldcore_load_sound_novel_files. Entry i is the start sector of chapter i relative to
 * the disc LBA below, so entry i+1 minus entry i is that chapter's length. */

void wldcore_load_message_block_if_changed(s32 chapter) {
    g_world_text_message_section = g_wldcore_message_buffer;
    if (chapter != g_wldcore_loaded_message_block) {
        wldcore_wait_for_file_load();
        g_wldcore_loaded_message_block = chapter;
        wldcore_wait_and_build_file_header(&g_main_file_cd_state,
            g_wldcore_message_block_sector_offsets[chapter] + 0x11f59,
            g_wldcore_message_block_sector_offsets[chapter + 1] - g_wldcore_message_block_sector_offsets[chapter],
            g_wldcore_message_buffer);
    }
}
