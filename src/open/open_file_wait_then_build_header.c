#include "fft/main_runtime.h"
#include "fft/open.h"

void open_file_wait_then_build_header(
    main_file_load_descriptor_t* header, s32 sector, s32 sector_count, void* destination) {
    open_file_wait_for_pending();
    main_file_build_header_nnl(header, sector, sector_count, destination);
}
