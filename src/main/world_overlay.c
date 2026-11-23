/* Load WORLD and WLDCORE into their separate overlay regions, then enter the world map.
 * Their blocking reads preserve or initialize the resident loading display. */

#include "fft/file.h"
#include "fft/memory.h"
#include "fft/overlay.h"
#include "fft/world_map.h"
#include "psx/libetc.h"
#include "psx/types.h"

extern u8* g_heap_world_overlay_load_address;
static void load_disc_sectors(
    int sector, int sectors, void* destination, file_loading_display_mode_e loading_display_mode);

/* Load WLDCORE.BIN into the low overlay area with the Now Loading screen and, when load_world is
 * set, WORLD.BIN at the world overlay address, then run WLDCORE's entry point and return its
 * result. */
world_map_result_e overlay_run_world(s32 load_world) {
    load_disc_sectors(DISC_SECTOR_WORLD_WLDCORE_BIN, 0xDC, g_heap_low_overlay_load_address, FILE_LOADING_DISPLAY_INIT);
    if (load_world != 0) {
        load_disc_sectors(
            DISC_SECTOR_WORLD_WORLD_BIN, 0x1E0, g_heap_world_overlay_load_address, FILE_LOADING_DISPLAY_PRESERVE);
    }
    return wldcore_entrypoint();
}

/* Load WORLD.BIN alone. mode - 1 is the loading display mode of
 * file_request_read, so mode 1 shows the "Now Loading" screen. */
void overlay_load_world(s32 mode) {
    load_disc_sectors(DISC_SECTOR_WORLD_WORLD_BIN, 0x1E0, g_heap_world_overlay_load_address, mode - 1);
}

/* Blocking read of whole sectors to destination through the shared loader. The
 * last argument is file_request_read's loading display mode: 0 blanks the
 * screen and sets up the "Now Loading" message, 1 leaves the display state alone,
 * and -1 turns the message off. */
static void load_disc_sectors(
    int sector, int sectors, void* destination, file_loading_display_mode_e loading_display_mode) {
    file_request_read(&g_file_cd_state, sector, sectors, destination, loading_display_mode);
    while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
        file_poll_load(&g_file_cd_state);
        VSync(0);
    }
}
