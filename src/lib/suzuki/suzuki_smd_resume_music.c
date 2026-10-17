#include "fft/suzuki.h"
#include "psx/libapi.h"
#include "psx/types.h"

/* Resumes a music handle paused by suzuki_smd_pause_music: with the
 * root-counter event disabled it fades the master volume in from 0,
 * re-applies the song's reverb mode, forces every channel's voice
 * parameters, retriggers held voices, then clears the pause bit and marks
 * the record playing. */
void suzuki_smd_resume_music(suzuki_music_t* music, s16 volume, s16 fade) {
    if (music != 0) {
        DisableEvent(g_suzuki_sound_root_counter_2_event);
        music->master_volume.value = 0;
        suzuki_smd_set_music_volume(music, volume, fade);
        suzuki_sound_set_reverb_mode(
            music->reverb_mode, music->reverb_depth, music->reverb_delay, music->reverb_feedback);
        suzuki_smd_set_voice_attr_flags_all_channels(music, 0x7000 | SUZUKI_VOICE_ATTR_ALL);
        suzuki_smd_retrigger_held_voices(music);
        music->status = (music->status & ~SUZUKI_MUSIC_STATUS_PAUSED) | SUZUKI_MUSIC_STATUS_PLAYING;
        EnableEvent(g_suzuki_sound_root_counter_2_event);
    }
}
