#include "fft/main.h"
#include "psx/types.h"

/* Keys off the playing scenario music and clears the current-music state;
 * its slot stays loaded. */
s32 main_sound_stop_current_music(void) {
    if (g_main_sound_music.state.current_music != 0) {
        suzuki_smd_stop_music(g_main_sound_music.state.current_music);
        g_main_sound_music.state.current_slot = 0;
        g_main_sound_music.state.current_smd = 0;
        g_main_sound_music.state.current_music = 0;
        return 1;
    }
    return 0;
}
