#include "fft/battle.h"
#include "psx/types.h"

/* Thread parameter 1 points at this five-byte fade request record. */
typedef struct battle_sound_bg_fade_request {
    u8 sound_id;    /* 0x0 */
    u8 volume_from; /* 0x1 */
    u8 volume_to;   /* 0x2 */
    u8 _unused_03;  /* 0x3 */
    u8 steps;       /* 0x4 */
} battle_sound_bg_fade_request_t;

void battle_sound_edit_bg_thread(void) {
    battle_sound_bg_fade_request_t* request;
    s32 sound_id;
    s32 volume_from;
    s32 volume_to;
    s32 steps;
    s32 delta;
    s32 scaled;
    s32 volume;
    s32 step;
    s32 bank;
    /* The target reserves eight frame bytes that no other local
     * accounts for; the pad reproduces the 0x40-byte frame. */
    s32 stack_pad[2];

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_BG_SOUND);
    step = 0;
    request = (battle_sound_bg_fade_request_t*)battle_thread_get_current_parameter_1();
    sound_id = request->sound_id;
    volume_from = request->volume_from;
    steps = request->steps;
    volume_to = request->volume_to;
    bank = 0x10000;
    if (steps != 0) {
        delta = volume_to - volume_from;
        scaled = 0;
        do {
            battle_thread_yield();
            volume = (scaled / steps) + volume_from;
            if (volume < 0) {
                volume = -volume;
            }
            if (volume == 0) {
                volume = 1;
            }
            scaled += delta;
            main_sound_set_sfx_echo(sound_id + bank, volume);
            step++;
        } while (step < steps);
    }
    main_sound_set_sfx_echo(sound_id | bank, volume_to);
    battle_thread_exit_current();
}
