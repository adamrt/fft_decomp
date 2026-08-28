#include "fft/main_file.h"

/*
 * Start an asynchronous disc read through the shared file header.
 *
 * Overlay callers pass a byte count; the lower-level builder takes sectors.
 */
s32 main_file_call_build_header(s32 sector, s32 size, void* destination) {
    return main_file_build_header_nnl(&g_main_file_cd_state, sector, (u32)size >> 11, destination);
}
