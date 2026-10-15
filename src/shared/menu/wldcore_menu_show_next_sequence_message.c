#include "fft/thread.h"
#include "fft/wldcore.h"

/* Show the countdown's next message and count it off; the last one also
 * fades the music out and moves the countdown to
 * WLDCORE_COUNTDOWN_PHASE_LAST_MESSAGE with the timer cleared. */
void wldcore_menu_show_next_sequence_message(wldcore_menu_countdown_t* countdown) {
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, countdown->current, 0);
    countdown->current++;
    countdown->remaining--;
    if (countdown->remaining == 0) {
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 4);
        countdown->phase = WLDCORE_COUNTDOWN_PHASE_LAST_MESSAGE;
        countdown->timer = 0;
    } else {
        countdown->phase = WLDCORE_COUNTDOWN_PHASE_MESSAGES;
    }
}
