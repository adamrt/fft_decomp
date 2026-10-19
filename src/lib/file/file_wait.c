/* Waiting for file loads, shared by the opening and the world map; the world map adds a blocking scratch load. */

#include "fft/file.h"
#include "fft/music.h"
#include "fft/system.h"
#include "fft/world_map.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* Start an asynchronous read of sector_count sectors at `sector` into
 * destination, once any read still pending has finished. */
void file_wait_then_request_read(file_load_descriptor_t* header, s32 sector, s32 sector_count, void* destination) {
    file_wait_for_pending();
    file_request_read_quiet(header, sector, sector_count, destination);
}

#ifdef FFT_WLDCORE
/* Read the map texture WLDTEX.TM2 (134 sectors) into g_world_map_scratch_buffer,
 * polling with world_map_poll_texture_stream until the read completes.
 * Skipped when the core is restarting (SYSTEM_FLAG_RESTART).
 *
 * The original 0x20-byte frame holds 16 bytes of outgoing arguments, the
 * ra/s0 save pair and eight bytes this body never references. The array below
 * preserves that frame size; the original local declarations are unknown. */
void world_map_load_texture(void) {
    s32 unused_stack_slot[2];

    if ((g_system_flags & SYSTEM_FLAG_RESTART) == 0) {
        file_wait_then_request_read(&g_file_cd_state, DISC_SECTOR_WORLD_WLDTEX_TM2, 0x86, g_world_map_scratch_buffer);
        while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
            world_map_poll_texture_stream(&g_file_cd_state);
        }
    }
}
#endif

/* Poll the pending CD load to completion, a frame at a time. WLDCORE also services its music queue; OPEN has no queue
 * here. */
void file_wait_for_pending(void) {
    while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
        file_poll_load(&g_file_cd_state);
#ifdef FFT_WLDCORE
        music_queue_process();
#endif
        VSync(0);
    }
}
