#include "fft/main_file.h"

void main_file_load_data_from_disc(int sector, int sectors, void* destination, int suppress_loading_display) {
    main_file_load_descriptor_t* descriptor = &g_main_file_cd_state;

    main_file_build_header(descriptor, sector, sectors, destination, suppress_loading_display);
    while (g_main_file_still_loading != 0) {
        main_file_poll_load(descriptor);
        VSync(0);
    }
}
