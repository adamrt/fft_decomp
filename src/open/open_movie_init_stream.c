#include "fft/open.h"
#include "psx/cd.h"
#include "psx/types.h"

void open_movie_init_stream(s32 sector, void* output_callback) {
    main_file_load_descriptor_t* header;

    open_bin_decdctreset(0);
    open_bin_decdctoutcallback(output_callback);
    StSetRing(g_open_movie_cd_stream_ring_buffer, 0x30);
    StSetStream(1, 0, -1, 0, 0);

    header = &g_open_file_header;
    main_file_build_header_nnl(header, sector, 1, 0);
    while (g_main_file_still_loading != 0) {
        main_file_poll_load(header);
        VSync(0);
    }

    while (CdControl(2, g_open_file_location, 0) == 0) { }
}
