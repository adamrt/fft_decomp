#include "fft/main_runtime.h"
#include "fft/open.h"

/* OPENBK images live in one contiguous sector run starting at 0x15243. */
#define OPEN_OPENBK_BASE_SECTOR 0x15243

void open_file_build_openbk_header(s32 image_id, void* destination) {
    s32 start_offset;
    s32 end_offset;
    s32 stack_padding[2];

    if (image_id == g_open_file_current_openbk_image_id) {
        return;
    }

    g_open_file_current_openbk_image_id = image_id;
    start_offset = g_open_file_openbk_start_sector_offsets[image_id];
    end_offset = g_open_file_openbk_end_sector_offsets[image_id];
    open_file_wait_then_build_header(
        &g_open_file_header, OPEN_OPENBK_BASE_SECTOR + start_offset, end_offset - start_offset, destination);
}
