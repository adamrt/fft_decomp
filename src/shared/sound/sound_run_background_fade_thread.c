#include "fft/main.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Thread task 0x35: ramp the volume of an environment sound from volume_from to
 * volume_to over `steps` yields, then settle on the final level and exit the
 * thread.
 *
 * A step's volume is never 0, which would stop the sound: a negative level is
 * negated and 0 becomes 1. The event script's edit-background-sound instruction
 * starts it as a thread, and sound_run_background_sfx_thread continues in it. */
void sound_run_background_fade_thread(void) {
    event_background_sound_parameters_t* request;
    s32 sound_id;
    s32 volume_from;
    s32 volume_to;
    s32 steps;
    s32 delta;
    s32 scaled;
    s32 volume;
    s32 step;
    s32 resource;
    /* The target reserves eight frame bytes that no other local
     * accounts for; the pad reproduces the 0x40-byte frame. */
    s32 stack_pad[2];

    thread_set_current_task_id(THREAD_TASK_BG_SOUND);
    step = 0;
    request = (event_background_sound_parameters_t*)thread_get_current_parameter_1_variant();
    sound_id = request->sound_id;
    volume_from = request->volume_from;
    steps = request->frames;
    volume_to = request->volume_to;
    resource = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, 0);
    if (steps != 0) {
        delta = volume_to - volume_from;
        scaled = 0;
        do {
            thread_yield();
            volume = (scaled / steps) + volume_from;
            if (volume < 0) {
                volume = -volume;
            }
            if (volume == 0) {
                volume = 1;
            }
            scaled += delta;
            main_sound_set_sfx_echo(sound_id + resource, volume);
            step++;
        } while (step < steps);
    }
    main_sound_set_sfx_echo(sound_id | resource, volume_to);
    thread_exit_current();
}
