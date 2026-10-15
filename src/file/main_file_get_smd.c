#include "fft/main.h"
#include "psx/types.h"

/* Blocking read of a sound file into a new block of the music heap. size is in
 * bytes and only whole sectors are read. A full heap reports
 * MAIN_SYSTEM_ERROR_SMD_LOAD and returns 0; the loader then only seeks, because
 * it skips the read for a null destination. */
suzuki_smd_header_t* main_file_get_smd(int sector, u32 size) {
    suzuki_smd_header_t* buffer;
    main_file_load_descriptor_t* header;

    buffer = main_heap_alloc_smd(size);
    if (buffer == 0) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_SMD_LOAD);
    }
    while (main_file_build_header_nnl(&g_main_file_cd_state, sector, size >> 11, buffer) != 0) {
        VSync(0);
        main_system_frame_stub();
        main_file_poll_load(&g_main_file_cd_state);
    }
    if (g_main_file_still_loading != 0) {
        header = &g_main_file_cd_state;
        do {
            VSync(0);
            main_system_frame_stub();
            main_file_poll_load(header);
        } while (g_main_file_still_loading != 0);
    }
    return buffer;
}
