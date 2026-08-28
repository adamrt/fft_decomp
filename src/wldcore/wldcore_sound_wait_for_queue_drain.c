#include "fft/main_file.h"
#include "fft/wldcore.h"

/* Twin of wldcore_wait_for_file_load (0x800686c8) with the audio-queue depth
 * as the wait condition. The two busy words are read as g_wldcore_audio_queue members
 * rather than as separate scalars. */
void wldcore_sound_wait_for_queue_drain(void) {
    while (g_wldcore_audio_queue.count != 0 || g_wldcore_audio_queue.current_command != 0) {
        main_file_poll_load(&g_main_file_cd_state);
        wldcore_sound_process_audio_queue();
        VSync(0);
    }
}
