#include "fft/wldcore.h"

/* Poll the CD load to completion, a frame at a time, keeping the audio queue
 * serviced. */
void file_wait_for_pending_variant(void) {
    while (g_main_file_cd_state.state != MAIN_FILE_LOAD_STATE_IDLE) {
        main_file_poll_load(&g_main_file_cd_state);
        wldcore_sound_process_audio_queue();
        VSync(0);
    }
}
