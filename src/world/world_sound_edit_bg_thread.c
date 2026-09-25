#include "fft/world.h"
#include "psx/types.h"

/* Thread task 0x35 continuation: ramp the parameter sound's echo level from
 * parameter[1] to parameter[2] over parameter[4] yields, then settle on the
 * final level and exit the thread. */
void world_sound_edit_bg_thread(void) {
    u8* parameter;
    s32 sound_id;
    s32 start_echo;
    s32 end_echo;
    s32 steps;
    s32 delta;
    s32 accumulator;
    s32 step;
    s32 echo;
    s32 sound_bank;
    u8 unused[8]; /* retail frame reserves eight bytes of locals */

    world_thread_set_current_task_id(NATIVE_THREAD_TASK_BG_SOUND);
    step = 0;
    parameter = world_thread_get_current_parameter_1();
    sound_id = parameter[0];
    start_echo = parameter[1];
    steps = parameter[4];
    end_echo = parameter[2];
    sound_bank = 0x10000;
    if (steps != 0) {
        delta = end_echo - start_echo;
        accumulator = 0;
        do {
            world_thread_yield();
            echo = (accumulator / steps) + start_echo;
            if (echo < 0) {
                echo = -echo;
            }
            if (echo == 0) {
                echo = 1;
            }
            accumulator += delta;
            main_sound_set_sfx_echo(sound_id + sound_bank, echo);
            step++;
        } while (step < steps);
    }
    main_sound_set_sfx_echo(sound_id | sound_bank, end_echo);
    world_thread_exit_current();
}
