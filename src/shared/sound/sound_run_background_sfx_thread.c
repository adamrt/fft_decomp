#include "fft/main.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Thread task 0x35, started by the event script's background-sound instruction:
 * restart an environment sound (sound resource ENV), on a free SFX channel pair
 * when find_channel is set and as the remembered background sound otherwise, and
 * set its volume (0 becomes 1).
 *
 * It then continues in sound_run_background_fade_thread, which reads the same
 * parameter record to fade from volume_from to volume_to over frames yields. */
void sound_run_background_sfx_thread(void) {
    event_background_sound_parameters_t* request;
    s32 sound_id;
    s32 sound;
    s32 volume;
    s32 find_channel;
    s32 resource = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, 0);

    thread_set_current_task_id(THREAD_TASK_BG_SOUND);
    request = (event_background_sound_parameters_t*)thread_get_current_parameter_1_variant();
    find_channel = request->find_channel;
    sound_id = request->sound_id;
    if (find_channel != 0) {
        sound = sound_id + resource;
        main_sound_stop_sfx(sound);
        main_sound_play_sfx_find_channel(sound);
    } else {
        sound = sound_id + resource;
        main_sound_stop_sfx(sound);
        main_sound_play_bg_sfx(sound);
    }
    volume = request->volume_from;
    if (volume == 0) {
        volume = 1;
    }
    main_sound_set_sfx_echo(sound_id | resource, volume);
    sound_run_background_fade_thread();
}
