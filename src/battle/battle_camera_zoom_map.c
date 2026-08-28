#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/main_sound.h"
#include "psx/types.h"

void battle_camera_zoom_map(s32 mode) {
    switch (mode) {
    case 0:
        if (g_battle_camera_zoom.vx > ONE) {
            g_battle_map_zoom_target = 1;
            g_battle_camera_zoom_action = 4;
            g_battle_camera_zoom_increment = 4;
            main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_OUT);
            return;
        }
        if (g_battle_camera_zoom.vx < ONE) {
            g_battle_map_zoom_target = 1;
            g_battle_camera_zoom_action = 2;
            g_battle_camera_zoom_increment = 4;
            main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_IN);
            return;
        }
        return;
    case 1:
        if (g_battle_camera_zoom.vx > 0xC00) {
            g_battle_map_zoom_target = 4;
            g_battle_camera_zoom_action = 4;
            g_battle_camera_zoom_increment = 4;
            main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_OUT);
            return;
        }
        if (g_battle_camera_zoom.vx < 0xC00) {
            g_battle_map_zoom_target = 4;
            g_battle_camera_zoom_action = 2;
            g_battle_camera_zoom_increment = 4;
            main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_IN);
            return;
        }
        return;
    }
}
