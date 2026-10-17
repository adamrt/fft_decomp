#include "fft/suzuki.h"
#include "psx/libspu.h"
#include "psx/types.h"

/* Pushes the voice attributes marked in each channel's voice_attr_flags to
 * the SPU, for every MUS record that is playing or owes a full voice update
 * and is not excluded by SUZUKI_MUSIC_STATUS_NO_VOICE_ATTRS, then clears the
 * marks. Voices reserved or pending release by SFX are skipped except for
 * the SFX record.
 *
 * The PITCH, ATTACK and SUSTAIN marks double as the marks set by the LFO,
 * noise and reverb opcodes (suzuki_smd_enable_lfo, suzuki_smd_turn_on_noise,
 * suzuki_smd_enable_reverb), so they also request the pitch-LFO, noise and
 * reverb voice-mask recalculations (g_suzuki_sound_voice_update_requests). The
 * volume/mode call already sets the volume, so VOLUME is dropped after it. */
void suzuki_sound_update_voice_attrs(void) {
    suzuki_music_t* music = g_suzuki_sound_active_music_list;
    suzuki_music_channel_t* channel;
    s16 status;
    s32 count;
    u32 allowed;
    u16 flags;
    s32 voice;

    while (music != 0) {
        status = music->status;
        if ((status & (SUZUKI_MUSIC_STATUS_FULL_VOICE_UPDATE | SUZUKI_MUSIC_STATUS_PLAYING)) != 0
            && (status & SUZUKI_MUSIC_STATUS_NO_VOICE_ATTRS) == 0) {
            music->status = status & ~SUZUKI_MUSIC_STATUS_FULL_VOICE_UPDATE;
            count = music->channel_count;
            channel = music->channels;
            if ((status & SUZUKI_MUSIC_STATUS_SFX) != 0)
                allowed = -1;
            else
                allowed = ~(g_suzuki_sound_sfx_reserved_voices | g_suzuki_sound_sfx_key_off_voices);
            do {
                if ((channel->flags & SUZUKI_CHANNEL_IN_USE) != 0 && (allowed & channel->voice_attr.voice) != 0) {
                    flags = channel->voice_attr_flags;
                    if (flags != 0) {
                        voice = channel->voice;
                        if ((flags & SUZUKI_VOICE_ATTR_VOLUME_MODE) != 0) {
                            SpuSetVoiceVolumeAttr(voice, channel->voice_attr.volume.left,
                                channel->voice_attr.volume.right, channel->voice_attr.volmode.left,
                                channel->voice_attr.volmode.right);
                            flags &= ~SUZUKI_VOICE_ATTR_VOLUME;
                        }
                        if ((flags & SUZUKI_VOICE_ATTR_VOLUME) != 0)
                            SpuSetVoiceVolume(voice, channel->voice_attr.volume.left, channel->voice_attr.volume.right);
                        if ((flags & SUZUKI_VOICE_ATTR_PITCH) != 0)
                            SpuSetVoicePitch(voice, channel->voice_attr.pitch);
                        if ((flags & SUZUKI_VOICE_ATTR_ADDRESS) != 0) {
                            SpuSetVoiceStartAddr(voice, channel->voice_attr.addr);
                            SpuSetVoiceLoopStartAddr(voice, channel->voice_attr.loop_addr);
                        }
                        if ((flags & SUZUKI_VOICE_ATTR_ATTACK) != 0)
                            SpuSetVoiceARAttr(voice, channel->voice_attr.ar, channel->voice_attr.a_mode);
                        if ((flags & SUZUKI_VOICE_ATTR_DECAY) != 0)
                            SpuSetVoiceDR(voice, channel->voice_attr.dr);
                        if ((flags & SUZUKI_VOICE_ATTR_SUSTAIN) != 0)
                            SpuSetVoiceSRAttr(voice, channel->voice_attr.sr, channel->voice_attr.s_mode);
                        if ((flags & SUZUKI_VOICE_ATTR_RELEASE) != 0)
                            SpuSetVoiceRRAttr(voice, channel->voice_attr.rr, channel->voice_attr.r_mode);
                        if ((flags & SUZUKI_VOICE_ATTR_SUSTAIN_LEVEL) != 0)
                            SpuSetVoiceSL(voice, channel->voice_attr.sl);
                        if ((flags & SUZUKI_VOICE_ATTR_PITCH) != 0)
                            g_suzuki_sound_voice_update_requests |= SUZUKI_VOICE_MASK_REQUEST_PITCH_LFO;
                        if ((flags & SUZUKI_VOICE_ATTR_ATTACK) != 0)
                            g_suzuki_sound_voice_update_requests |= SUZUKI_VOICE_MASK_REQUEST_NOISE;
                        if ((flags & SUZUKI_VOICE_ATTR_SUSTAIN) != 0)
                            g_suzuki_sound_voice_update_requests |= SUZUKI_VOICE_MASK_REQUEST_REVERB;
                        channel->voice_attr_flags = 0;
                    }
                }
                channel++;
            } while (--count != 0);
        }
        music = music->next;
    }
}
