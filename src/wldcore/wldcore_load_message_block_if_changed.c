#include "fft/wldcore.h"

/* Loads message block `block` (wldcore_message_block_e) unless it is the one already loaded.
 * g_wldcore_message_block_sector_offsets has the same shape as the pair used by
 * wldcore_load_sound_novel_files: entry i is the start sector of block i relative to the disc
 * LBA below, so entry i+1 minus entry i is that block's length. */

void wldcore_load_message_block_if_changed(s32 block) {
    g_text_section_pointers[TEXT_SECTION_MESSAGES] = g_wldcore_message_buffer;
    if (block != g_wldcore_loaded_message_block) {
        file_wait_for_pending_variant();
        g_wldcore_loaded_message_block = block;
        file_wait_then_build_header(&g_main_file_cd_state,
            g_wldcore_message_block_sector_offsets[block] + DISC_SECTOR_WORLD_WLDMES_BIN,
            g_wldcore_message_block_sector_offsets[block + 1] - g_wldcore_message_block_sector_offsets[block],
            g_wldcore_message_buffer);
    }
}
