/* Event-script threads for starting an environment sound and stepping its volume fade. */

#include "fft/script_instruction.h"
#include "fft/sound.h"
#include "fft/suzuki.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Thread task 0x35, started by the event script's background-sound instruction:
 * restart an environment sound (sound resource ENV), on a free SFX channel pair
 * when find_channel is set and as the remembered background sound otherwise, and
 * set its volume (0 becomes 1).
 *
 * It then continues in sound_background_fade_thread_run, which reads the same
 * parameter record to fade from volume_from to volume_to over frames yields. */
void sound_background_thread_run(void) {
    event_background_sound_parameters_t* request;
    s32 sound_id;
    s32 sound;
    s32 volume;
    s32 find_channel;
    s32 resource = SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, 0);

    thread_current_task_set(THREAD_TASK_BG_SOUND);
    request = (event_background_sound_parameters_t*)thread_current_parameter_1_get();
    find_channel = request->find_channel;
    sound_id = request->sound_id;
    if (find_channel != 0) {
        sound = sound_id + resource;
        sound_sfx_stop(sound);
        sound_free_channel_sfx_play(sound);
    } else {
        sound = sound_id + resource;
        sound_sfx_stop(sound);
        sound_background_play(sound);
    }
    volume = request->volume_from;
    if (volume == 0) {
        volume = 1;
    }
    sound_sfx_volume_set(sound_id | resource, volume);
    sound_background_fade_thread_run();
}

/* Thread task 0x35: ramp the volume of an environment sound from volume_from to
 * volume_to over `steps` yields, then settle on the final level and exit the
 * thread.
 *
 * A step's volume is never 0, which would stop the sound: a negative level is
 * negated and 0 becomes 1. The event script's edit-background-sound instruction
 * starts it as a thread, and sound_background_thread_run continues in it. */
void sound_background_fade_thread_run(void) {
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

    thread_current_task_set(THREAD_TASK_BG_SOUND);
    step = 0;
    request = (event_background_sound_parameters_t*)thread_current_parameter_1_get();
    sound_id = request->sound_id;
    volume_from = request->volume_from;
    steps = request->frames;
    volume_to = request->volume_to;
    resource = SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, 0);
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
            sound_sfx_volume_set(sound_id + resource, volume);
            step++;
        } while (step < steps);
    }
    sound_sfx_volume_set(sound_id | resource, volume_to);
    thread_current_exit();
}
