#include "fft/main.h"
#include "psx/spu.h"
#include "psx/types.h"

/* Pushes the voice attributes marked in each channel's func_flags to the SPU,
 * for every MUS record that is playing or restarting (status 0xc000) and not
 * paused (0x20), then clears the marks. Voices reserved or pending release by
 * SFX are skipped except for the SFX record (status bit 1).
 *
 * func_flags 0x4, 0x10 and 0x40 double as the marks set by the LFO, noise and
 * reverb opcodes (main_smd_enable_lfo, main_smd_turn_on_noise,
 * main_smd_enable_reverb), so they also request the pitch-LFO, noise and
 * reverb voice-mask recalculations (g_main_sound_voice_update_requests bits 0-2). The volume/mode call
 * already sets the volume, so bit 0 is dropped after it. */
void main_sound_update_voice_attrs(void) {
    suzuki_music_t* music;
    suzuki_music_channel_t* channel;
    s16 status;
    s32 count;
    u32 allowed;
    u16 flags;
    s32 voice;

    music = g_main_sound_active_music_list;
    while (music != 0) {
        status = music->status;
        if ((status & 0xc000) != 0 && (status & 0x20) == 0) {
            music->status = status & ~0x4000;
            count = music->channel_count;
            channel = music->channels;
            if ((status & 2) != 0)
                allowed = -1;
            else
                allowed = ~(g_main_mask_exclusion | g_main_sound_sfx_key_off_voices);
            do {
                if ((channel->active & 1) != 0 && (allowed & channel->voice_mask) != 0) {
                    flags = channel->func_flags;
                    if (flags != 0) {
                        voice = channel->voice;
                        if ((flags & 2) != 0) {
                            SpuSetVoiceVolumeAttr(voice, channel->spu_volume_left, channel->spu_volume_right,
                                channel->spu_volume_mode_left, channel->spu_volume_mode_right);
                            flags &= ~1;
                        }
                        if ((flags & 1) != 0)
                            SpuSetVoiceVolume(voice, channel->spu_volume_left, channel->spu_volume_right);
                        if ((flags & 4) != 0)
                            SpuSetVoicePitch(voice, channel->spu_pitch);
                        if ((flags & 8) != 0) {
                            SpuSetVoiceStartAddr(voice, channel->start_address);
                            SpuSetVoiceLoopStartAddr(voice, channel->loop_address);
                        }
                        if ((flags & 0x10) != 0)
                            SpuSetVoiceARAttr(voice, channel->attack_time, channel->attack_mode);
                        if ((flags & 0x20) != 0)
                            SpuSetVoiceDR(voice, channel->decay_time);
                        if ((flags & 0x40) != 0)
                            SpuSetVoiceSRAttr(voice, channel->sustain_time, channel->sustain_mode);
                        if ((flags & 0x80) != 0)
                            SpuSetVoiceRRAttr(voice, channel->release_time, channel->release_mode);
                        if ((flags & 0x100) != 0)
                            SpuSetVoiceSL(voice, channel->sustain_level);
                        if ((flags & 4) != 0)
                            g_main_sound_voice_update_requests |= 1;
                        if ((flags & 0x10) != 0)
                            g_main_sound_voice_update_requests |= 2;
                        if ((flags & 0x40) != 0)
                            g_main_sound_voice_update_requests |= 4;
                        channel->func_flags = 0;
                    }
                }
                channel++;
            } while (--count != 0);
        }
        music = music->next;
    }
}
