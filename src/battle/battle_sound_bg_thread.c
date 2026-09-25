#include "fft/thread.h"
#include "psx/types.h"

/* Thread parameter 1 points at this four-byte request record. */
typedef struct battle_sound_bg_request {
    u8 sound_id;     /* 0x0 */
    u8 volume;       /* 0x1 */
    u8 unknown_02;   /* 0x2 */
    u8 find_channel; /* 0x3 */
} battle_sound_bg_request_t;

void battle_sound_bg_thread(void) {
    battle_sound_bg_request_t* request;
    s32 sound_id;
    s32 sound;
    s32 volume;
    s32 find_channel;
    s32 bank = 0x10000;

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_BG_SOUND);
    request = (battle_sound_bg_request_t*)battle_thread_get_current_parameter_1();
    find_channel = request->find_channel;
    sound_id = request->sound_id;
    if (find_channel != 0) {
        sound = sound_id + bank;
        main_sound_stop_sfx(sound);
        main_sound_play_sfx_find_channel(sound);
    } else {
        sound = sound_id + bank;
        main_sound_stop_sfx(sound);
        main_sound_play_weather_sfx(sound);
    }
    volume = request->volume;
    if (volume == 0) {
        volume = 1;
    }
    main_sound_set_sfx_echo(sound_id | bank, volume);
    battle_sound_edit_bg_thread();
}
