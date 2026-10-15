#include "fft/open.h"

/* Start reading image image_id of OPNBK.BIN into destination, unless it is the
 * image read last (g_open_file_current_openbk_image_id). The read is
 * asynchronous; only a read already pending is waited for.
 *
 * The end-offset table is the start-offset table one entry further on. By their
 * sector counts images 0 and 1 are 320x240 and 256x240 frame images, 2 to 5 are
 * 210x180 pictures and 6 is the 512x240 ending picture. */
void open_file_build_openbk_header(s32 image_id, void* destination) {
    s32 start_offset;
    s32 stack_padding[2];

    if (image_id == g_open_file_current_openbk_image_id) {
        return;
    }

    g_open_file_current_openbk_image_id = image_id;
    start_offset = g_open_file_openbk_start_sector_offsets[image_id];
    file_wait_then_build_header(&g_main_file_cd_state, DISC_SECTOR_OPEN_OPNBK_BIN + start_offset,
        g_open_file_openbk_end_sector_offsets[image_id] - start_offset, destination);
}
