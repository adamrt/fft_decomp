#include "fft/main.h"
#include "fft/wldcore.h"

s32 main_overlay_open_world_and_wldcore(s32 load_world) {
    main_file_load_data_from_disc(
        DISC_SECTOR_WORLD_WLDCORE_BIN, 0xDC, g_main_heap_low_overlay_load_address, MAIN_FILE_LOADING_DISPLAY_INIT);
    if (load_world != 0) {
        main_file_load_data_from_disc(DISC_SECTOR_WORLD_WORLD_BIN, 0x1E0, g_main_heap_world_overlay_load_address,
            MAIN_FILE_LOADING_DISPLAY_PRESERVE);
    }
    return wldcore_entrypoint();
}

/* Load WORLD.BIN alone. mode - 1 is the loading display mode of
 * main_file_build_header, so mode 1 shows the "Now Loading" screen. */
void main_overlay_open_world_bin(s32 mode) {
    main_file_load_data_from_disc(DISC_SECTOR_WORLD_WORLD_BIN, 0x1E0, g_main_heap_world_overlay_load_address, mode - 1);
}

/* Blocking read of whole sectors to destination through the shared loader. The
 * last argument is main_file_build_header's loading display mode: 0 blanks the
 * screen and sets up the "Now Loading" message, 1 leaves the display state alone,
 * and -1 turns the message off. */
void main_file_load_data_from_disc(int sector, int sectors, void* destination, int loading_display_mode) {
    main_file_load_descriptor_t* descriptor = &g_main_file_cd_state;

    main_file_build_header(descriptor, sector, sectors, destination, loading_display_mode);
    while (g_main_file_still_loading != 0) {
        main_file_poll_load(descriptor);
        VSync(0);
    }
}
