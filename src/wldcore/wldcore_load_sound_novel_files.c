#include "fft/wldcore.h"

/* Load sound-novel chapter `chapter`: its script into the save slot buffer
 * and its text into the message buffer, which becomes the message section.
 *
 * Each file comes from a seven-entry sector-offset table. An entry is the
 * start sector of one chapter's data relative to the disc LBA added to it,
 * so entry i+1 minus entry i is that chapter's sector count. */
void wldcore_load_sound_novel_files(s32 chapter) {
    file_wait_for_pending_variant();
    file_wait_then_build_header(&g_main_file_cd_state,
        g_wldcore_sound_novel_script_sector_offsets[chapter] + DISC_SECTOR_WORLD_SNPLBIN_BIN,
        g_wldcore_sound_novel_script_sector_offsets[chapter + 1] - g_wldcore_sound_novel_script_sector_offsets[chapter],
        &g_main_save_slot_buffer);
    file_wait_for_pending_variant();
    file_wait_then_build_header(&g_main_file_cd_state,
        g_wldcore_sound_novel_text_sector_offsets[chapter] + DISC_SECTOR_WORLD_SNPLMES_BIN,
        g_wldcore_sound_novel_text_sector_offsets[chapter + 1] - g_wldcore_sound_novel_text_sector_offsets[chapter],
        g_wldcore_message_buffer);
    g_text_section_pointers[TEXT_SECTION_MESSAGES] = g_wldcore_message_buffer;
    g_wldcore_loaded_message_block = -1;
}
