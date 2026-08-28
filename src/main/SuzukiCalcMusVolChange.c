#include "fft/main_sound.h"
#include "psx/api.h"
#include "psx/etc.h"
#include "psx/types.h"

/* Sets the music's master volume, immediately when time is 0, otherwise as
 * a ramp over time ticks. A non-zero volume also resumes music paused by
 * main_smd_pause_music, like main_smd_resume_music. */
void SuzukiCalcMusVolChange(suzuki_music_t* music, s16 volume, s16 time) {
    s32 difference;

    music->master_volume.target = volume << 8;
    if (time == 0) {
        music->master_volume.value = volume << 24;
        music->master_volume.count = 0;
        main_smd_set_note_flags2_all_channels(0x100, music);
    } else {
        difference = (volume << 16) - (music->master_volume.value >> 8);
        if (difference == 0) {
            return;
        }
        music->master_volume.count = time;
        music->master_volume.step = (difference / time) << 8;
    }
    if ((music->status & 0x100) && volume != 0) {
        DisableEvent(g_main_root_counter_2_event);
        main_sound_set_reverb_mode(
            music->reverb_mode, music->reverb_depth, music->reverb_delay, music->reverb_feedback);
        main_smd_force_channel_func(music, 0x71ff);
        main_smd_retrigger_held_voices(music);
        music->status = (music->status & ~0x100) | 0x8000;
        EnableEvent(g_main_root_counter_2_event);
    }
}
