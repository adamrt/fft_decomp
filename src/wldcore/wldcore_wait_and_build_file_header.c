#include "fft/wldcore.h"

void wldcore_wait_and_build_file_header(
    main_file_load_descriptor_t* header, s32 sector, s32 sectors, void* destination) {
    wldcore_wait_for_file_load();
    main_file_build_header_nnl(header, sector, sectors, destination);
}
