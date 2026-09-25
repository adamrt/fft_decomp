#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Fades the screen colour modulation from a start RGB to an end RGB over a duration.
 *
 * Twin of world_script_color_screen_thread. Parameters: mode byte, start RGB, end RGB,
 * then a halfword duration in frames. The colour advances by two frames per
 * step and finally snaps to the end colour. */
void battle_script_color_screen_thread(void) {
    s32 color[3][3]; /* start, end, current RGB */
    u8* parameters;
    s32 mode;
    s32 i;
    s32 j;
    s32 duration;

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_COLOR_SCREEN);
    parameters = (u8*)battle_thread_get_current_parameter_1();
    mode = *parameters++;
    for (i = 0; i < 6; i++) {
        color[0][i] = *parameters++;
    }
    duration = battle_script_load_halfword(parameters);
    for (i = 0; i < duration; i += 2) {
        battle_thread_yield();
        battle_thread_yield();
        for (j = 0; j < 3; j++) {
            color[2][j] = (color[1][j] - color[0][j]) * i / duration + color[0][j];
        }
        battle_gfx_start_screen_color_modulation_fade(mode, color[2][0], color[2][1], color[2][2], 2);
    }
    for (j = 0; j < 3; j++) {
        color[2][j] = color[1][j];
    }
    battle_gfx_start_screen_color_modulation_fade(mode, color[2][0], color[2][1], color[2][2], 2);
    battle_thread_yield();
    battle_thread_yield();
    battle_thread_exit_current();
}
