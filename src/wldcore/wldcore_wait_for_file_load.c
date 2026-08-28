#include "fft/main_file.h"

void wldcore_wait_for_file_load(void) {
    while (g_main_file_cd_state.state != MAIN_FILE_LOAD_STATE_IDLE) {
        main_file_poll_load(&g_main_file_cd_state);
        wldcore_sound_process_audio_queue();
        VSync(0);
    }
}
