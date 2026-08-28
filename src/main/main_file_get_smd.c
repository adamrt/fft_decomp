#include "fft/main_file.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void* main_file_get_smd(int sector, int size) {
    void* buffer;
    main_file_load_descriptor_t* header;

    buffer = main_heap_alloc_smd(size);
    if (buffer == 0) {
        main_system_handle_animation_exception(0x11);
    }
    while (main_file_build_header_nnl(&g_main_file_cd_state, sector, (u32)size >> 11, buffer) != 0) {
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
    return buffer;
}
