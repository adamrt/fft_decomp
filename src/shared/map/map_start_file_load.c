#include "fft/battle_map.h"
#include "psx/types.h"

/* Start reading one map file into `destination`. Returns `destination` when
 * the read was started, 0 when the row names no file or the start failed. */
void* map_start_file_load(
    const char* unused_directory_path, const gns_load_info_t* load_info, void* destination, s32 unused_resource_type) {
    void* result;

    if (load_info->fields.file_present_marker == 0) {
        result = 0;
    } else {
        /* These 32-bit values are aligned within the file row; retail loads both with lw. */
        if (main_file_call_build_header(
                *(const s32*)load_info->fields.disc_lba_le, *(const s32*)load_info->fields.byte_length_le, destination)
            == 0) {
            return destination;
        }
        result = 0;
    }
    return result;
}
