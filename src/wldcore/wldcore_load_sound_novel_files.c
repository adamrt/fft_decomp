#include "fft/main_file.h"
#include "fft/main_runtime.h"
#include "fft/wldcore.h"

/* Provisional: two seven-entry sector-offset tables. Each entry is the start
 * sector of one chapter's data relative to the disc LBA below it, so entry
 * i+1 minus entry i is that chapter's sector count. */

void wldcore_load_sound_novel_files(s32 chapter) {
    wldcore_wait_for_file_load();
    wldcore_wait_and_build_file_header(&g_main_file_cd_state,
        g_wldcore_sound_novel_script_sector_offsets[chapter] + 0x128f9,
        g_wldcore_sound_novel_script_sector_offsets[chapter + 1] - g_wldcore_sound_novel_script_sector_offsets[chapter],
        g_main_save_slot_buffer);
    wldcore_wait_for_file_load();
    wldcore_wait_and_build_file_header(&g_main_file_cd_state,
        g_wldcore_sound_novel_text_sector_offsets[chapter] + 0x1291d,
        g_wldcore_sound_novel_text_sector_offsets[chapter + 1] - g_wldcore_sound_novel_text_sector_offsets[chapter],
        g_wldcore_message_buffer);
    g_world_text_message_section = g_wldcore_message_buffer;
    g_wldcore_loaded_message_block = -1;
}
