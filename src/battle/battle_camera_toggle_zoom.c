#include "fft/battle.h"
#include "psx/types.h"

/* Toggle the battle map zoom between "in" (target=1, action=2) and
   "out" (target=4, action=4), playing the corresponding zoom SFX. Any
   other current zoom target is left alone. */
void battle_camera_toggle_zoom(void) {
    s32 sound_id;
    s32 zoomed_in = 1;

    if (g_battle_map_zoom_target == zoomed_in) {
        g_battle_camera_zoom_action = 4;
        g_battle_map_zoom_target = 4;
        sound_id = MAIN_SFX_CAMERA_ZOOM_OUT;
        goto play;
    }
    sound_id = MAIN_SFX_CAMERA_ZOOM_IN;
    if (g_battle_map_zoom_target == 4) {
        g_battle_map_zoom_target = zoomed_in;
        g_battle_camera_zoom_action = 2;
    play:
        /* The target passes a second argument the one-parameter callee ignores. */
        ((void (*)(s32, s32))main_sound_play_sfx)(sound_id, zoomed_in);
    }
    g_battle_camera_zoom_increment = 4;
}
