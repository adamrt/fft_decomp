/* Waiting for file loads, shared by the opening and the world map; the world map adds a blocking scratch load. */

#include "fft/file.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/wldcore.h"
#include "fft/wldcore_script.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* Start an asynchronous read of sector_count sectors at `sector` into
 * destination, once any read still pending has finished. */
void file_wait_then_build_header(main_file_load_descriptor_t* header, s32 sector, s32 sector_count, void* destination) {
    file_wait_for_pending();
    main_file_request_read_quiet(header, sector, sector_count, destination);
}

#ifdef FFT_WLDCORE
/* Reads 134 sectors from LBA 0x11d28 into the wldcore scratch buffer unless
 * system flag 0x10000 is set, then spins the overlay's own poll routine until
 * the transfer completes.
 *
 * The target frame is 0x20 rather than the 0x18 this body needs: 16 bytes of
 * outgoing arguments plus the ra/s0 save pair leave 8 bytes of unreferenced
 * locals, so the original declared an 8-byte stack object it never used. Its
 * type is unknown; the array below reproduces the slot exactly. */
void wldcore_map_load_texture(void) {
    s32 unused_stack_slot[2];

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_RESTART) == 0) {
        file_wait_then_build_header(
            &g_main_file_cd_state, DISC_SECTOR_WORLD_WLDTEX_TM2, 0x86, g_wldcore_scratch_buffer);
        while (g_main_file_cd_state.state != MAIN_FILE_LOAD_STATE_IDLE) {
            wldcore_file_poll_vram_image_stream(&g_main_file_cd_state);
        }
    }
}
#endif

#ifdef FFT_WLDCORE
/* Poll the CD load to completion, a frame at a time, keeping the audio queue
 * serviced. */
void file_wait_for_pending(void) {
    while (g_main_file_cd_state.state != MAIN_FILE_LOAD_STATE_IDLE) {
        main_file_poll_load(&g_main_file_cd_state);
        wldcore_sound_process_audio_queue();
        VSync(0);
    }
}
#endif

#ifdef FFT_OPENING
/* Poll the pending file load to completion, a frame at a time; unlike the WLDCORE
 * body there is no audio queue to service. */
void file_wait_for_pending(void) {
    u32* status = &g_main_file_still_loading;
    main_file_load_descriptor_t* header;

    if (*status == 0) {
        return;
    }

    /* The flag aliases state; a separate descriptor symbol changes the load scheduling. */
    header = (main_file_load_descriptor_t*)((u8*)status - offsetof(main_file_load_descriptor_t, state));
    do {
        main_file_poll_load(header);
        VSync(0);
    } while (g_main_file_still_loading != 0);
}
#endif
