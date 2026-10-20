/* Wait for pending disc reads in OPEN and WLDCORE. WLDCORE also streams the
 * world-map texture into its scratch buffer through this unit. */

#include "fft/file.h"
#include "fft/music.h"
#include "fft/system.h"
#include "fft/world_map.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* Start an asynchronous read of sector_count sectors at `sector` into
 * destination, once any read still pending has finished. */
void file_pending_read_request(file_load_descriptor_t* header, s32 sector, s32 sector_count, void* destination) {
    file_pending_wait();
    file_read_request_quiet(header, sector, sector_count, destination);
}

#ifdef FFT_WLDCORE
/* Read the map texture WLDTEX.TM2 (134 sectors) into g_world_map_scratch_buffer,
 * polling with world_map_texture_stream_update until the read completes.
 * Skipped when the core is restarting (WORLD_MAP_FLAG_RESTART).
 *
 * The original 0x20-byte frame holds 16 bytes of outgoing arguments, the
 * ra/s0 save pair and eight bytes this body never references. The array below
 * preserves that frame size; the original local declarations are unknown. */
void world_map_load_texture(void) {
    s32 unused_stack_slot[2];

    if ((g_world_map_flags & WORLD_MAP_FLAG_RESTART) == 0) {
        file_pending_read_request(&g_file_cd_state, DISC_SECTOR_WORLD_WLDTEX_TM2, 0x86, g_world_map_scratch_buffer);
        while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
            world_map_texture_stream_update(&g_file_cd_state);
        }
    }
}
#endif

/* Poll the pending CD load to completion, a frame at a time. WLDCORE also services its music queue; OPEN has no queue
 * here. */
void file_pending_wait(void) {
    while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
        file_load_poll(&g_file_cd_state);
#ifdef FFT_WLDCORE
        music_queue_process();
#endif
        VSync(0);
    }
}
