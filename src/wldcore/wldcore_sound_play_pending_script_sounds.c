#include "fft/main_sound.h"
#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

/* Script opcode: play the requested sound, weather quad and music track, then
 * clear the three sound script variables.
 *
 * The switch tests the separately catalogued neighbour
 * g_wldcore_active_saved_record.sound_mode, not part of g_wldcore_active_saved_record.music_selection.
 * The selection byte read goes through *(u8*) so it stays an lbu as in the target. */
void wldcore_sound_play_pending_script_sounds(s32 sound_id, s32 weather_sound_id, s32 track) {
    s32* music_flags;

    if ((sound_id | weather_sound_id | track) == 0) {
        return;
    }
    switch ((s16)g_wldcore_active_saved_record.sound_mode) {
    case 0:
        if (sound_id != 0) {
            wldcore_sound_play_effect(sound_id);
        }
        if (weather_sound_id != 0) {
            wldcore_sound_stop_weather_sfx_and_send_quad(weather_sound_id);
        }
        break;
    case 1:
        if (sound_id != 0) {
            wldcore_sound_play_effect(sound_id);
        }
        if (weather_sound_id != 0) {
            wldcore_sound_stop_weather_sfx_and_send_quad(weather_sound_id);
        }
        if (track != 0) {
            if ((u8)g_wldcore_active_saved_record.music_selection != track) {
                wldcore_sound_change_scenario_music(track);
                music_flags = &g_wldcore_audio_queue.music.flags;
                *music_flags |= 1;
                main_sound_switch_music_track(((s32)(g_wldcore_active_saved_record.music_selection << 16)) >> 24, 0, 0);
                main_sound_set_current_music_target(g_wldcore_audio_queue.volume, 0x28);
            }
        }
        break;
    }
    world_script_set_variable(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
    world_script_set_variable(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
    world_script_set_variable(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
}
