#include "fft/open.h"

/* Start an asynchronous read of sector_count sectors at `sector` into
 * destination, once any read still pending has finished. */
void file_wait_then_build_header(main_file_load_descriptor_t* header, s32 sector, s32 sector_count, void* destination) {
    file_wait_for_pending_variant();
    main_file_build_header_nnl(header, sector, sector_count, destination);
}
