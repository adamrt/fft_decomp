#include "fft/main.h"

/* Returns the slot of the current music (g_main_sound_music.state.current_slot),
 * not a music id. */
int main_sound_get_current_scenario_music(void) {
    return g_main_sound_music.state.current_slot;
}
