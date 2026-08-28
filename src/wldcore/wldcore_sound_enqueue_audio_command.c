#include "fft/main_file.h"
#include "fft/wldcore.h"

/* Queue an audio command, servicing frame work while the queue is full.
 *
 * Accessing the count as a g_wldcore_audio_queue member is what makes GCC reload it
 * after each array store. */
void wldcore_sound_enqueue_audio_command(s32 kind, s32 value) {
    while (g_wldcore_audio_queue.count >= 16) {
        main_file_poll_load(&g_main_file_cd_state);
        wldcore_sound_process_audio_queue();
        VSync(0);
    }

    g_wldcore_audio_queue.commands[g_wldcore_audio_queue.count] = kind;
    g_wldcore_audio_queue.values[g_wldcore_audio_queue.count] = value;
    g_wldcore_audio_queue.count++;
}
