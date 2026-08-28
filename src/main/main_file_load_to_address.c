#include "fft/main_file.h"
#include "fft/main_runtime.h"

void* main_file_load_to_address(int sector, unsigned int size, void* destination) {
    main_file_load_descriptor_t* header;

    while (main_file_build_header_nnl(&g_main_file_cd_state, sector, size >> 11, destination) != 0) {
        VSync(0);
        main_noop_800449ec();
        main_file_poll_load(&g_main_file_cd_state);
    }
    if (g_main_file_still_loading != 0) {
        header = &g_main_file_cd_state;
        do {
            VSync(0);
            main_noop_800449ec();
            main_file_poll_load(header);
        } while (g_main_file_still_loading != 0);
    }
    return destination;
}
