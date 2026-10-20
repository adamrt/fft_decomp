/* Load WORLD and WLDCORE into their separate overlay regions, then enter the world map.
 * Their blocking reads preserve or initialize the resident loading display. */

#include "fft/file.h"
#include "fft/memory.h"
#include "fft/overlay.h"
#include "fft/world_map.h"
#include "psx/libetc.h"
#include "psx/types.h"

extern u8* g_heap_world_overlay_load_address;
static void disc_sectors_load(
    int sector, int sectors, void* destination, file_loading_display_mode_e loading_display_mode);

/* Load WLDCORE.BIN into the low overlay area with the Now Loading screen and, when load_world is
 * set, WORLD.BIN at the world overlay address, then run WLDCORE's entry point and return its
 * result. */
world_map_result_e overlay_world_run(s32 load_world) {
    disc_sectors_load(DISC_SECTOR_WORLD_WLDCORE_BIN, 0xDC, g_heap_low_overlay_load_address, FILE_LOADING_DISPLAY_INIT);
    if (load_world != 0) {
        disc_sectors_load(
            DISC_SECTOR_WORLD_WORLD_BIN, 0x1E0, g_heap_world_overlay_load_address, FILE_LOADING_DISPLAY_PRESERVE);
    }
    return wldcore_entrypoint();
}

/* Load WORLD.BIN alone. mode - 1 is the loading display mode of
 * file_read_request, so mode 1 shows the "Now Loading" screen. */
void overlay_world_load(s32 mode) {
    disc_sectors_load(DISC_SECTOR_WORLD_WORLD_BIN, 0x1E0, g_heap_world_overlay_load_address, mode - 1);
}

/* Blocking read of whole sectors to destination through the shared loader. The
 * last argument is file_read_request's loading display mode: 0 blanks the
 * screen and sets up the "Now Loading" message, 1 leaves the display state alone,
 * and -1 turns the message off. */
static void disc_sectors_load(
    int sector, int sectors, void* destination, file_loading_display_mode_e loading_display_mode) {
    file_read_request(&g_file_cd_state, sector, sectors, destination, loading_display_mode);
    while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
        file_load_poll(&g_file_cd_state);
        VSync(0);
    }
}
