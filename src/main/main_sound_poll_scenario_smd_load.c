#include "fft/main_sound.h"

/*
 * handles[0] holds the slot whose SMD load is in flight; handles[1..2] hold
 * the resulting music handles and smd[1..2] their source SMD data.
 */
int main_sound_poll_scenario_smd_load(void) {
    int* state = g_main_sound_music.slots.handles;

    if (*state == 0) {
        return 0;
    }
    if (main_file_is_still_loading() != 0) {
        return 1;
    }
    state[g_main_sound_music.slots.handles[0]]
        = (s32)SuzukiPutPlaySMD(g_main_sound_music.slots.smd[g_main_sound_music.slots.handles[0]]);
    *state = 0;
    return 0;
}
