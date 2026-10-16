#include "psx/suzuki.h"
#include "psx/types.h"

/* Per-tick voice pass run after the SMD interpreter and the modulators.
 * SUZUKI_VOICE_UPDATE_VOLUME rebuilds the SPU voice volume: channel volume
 * plus its modulation (clamped to 0-0x7fff), scaled by velocity and the MUS
 * master volume, then split by balance (channel, modulation and MUS balance
 * shift, clamped to 0-0x7f00) into left/right levels when the driver runs in
 * stereo, or both set to 0x5a00/0x8000 of the volume in mono.
 * SUZUKI_VOICE_UPDATE_PITCH converts the pitch (with its modulation and the
 * MUS pitch shift) to an SPU pitch. The VOLUME and PITCH voice attribute
 * marks hand the results to suzuki_sound_update_voice_attrs. KEY_ON and
 * KEY_OFF queue the channel's voices for key on (unless muted) and key off
 * in the MUS masks, filtered by the voices SFX own unless this is the SFX
 * record.
 *
 * The channel pointer is a copy of the parameter so the count parameter is
 * copied first in the prologue, as in the target. */
void suzuki_smd_update_voices(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count) {
    suzuki_music_channel_t* channel = channels;
    u32 key_on;
    u32 key_off;
    u32 allowed;
    u16 voice_update_flags;
    s32 volume;
    s32 balance;
    s32 left;
    s32 right;

    if ((music->status & SUZUKI_MUSIC_STATUS_SFX) == 0)
        allowed = ~(g_suzuki_sound_sfx_reserved_voices | g_suzuki_sound_sfx_key_off_voices);
    else
        allowed = -1;
    key_off = 0;
    key_on = 0;
    do {
        if (channel->flags != 0 && channel->voice < 25) {
            voice_update_flags = channel->voice_update_flags;
            if ((voice_update_flags & SUZUKI_VOICE_UPDATE_VOLUME) != 0) {
                volume = (channel->volume >> 16) + channel->volume_modulation;
                if (volume > 0x7fff)
                    volume = 0x7fff;
                if (volume < 0)
                    volume = 0;
                volume = (channel->velocity * volume) >> 15;
                volume = ((music->master_volume.value >> 16) * volume) >> 16;
                balance = channel->balance + channel->balance_modulation + (music->balance_shift.value >> 16);
                if (balance > 0x7f00)
                    balance = 0x7f00;
                if (balance < 0)
                    balance = 0;
                if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_STEREO) != 0) {
                    if (balance < 0x4000) {
                        right = (balance * 0x5a00) >> 14;
                        left = 0x7f00 - ((balance * 0x2500) >> 14);
                    } else {
                        balance = 0x8000 - balance;
                        left = (balance * 0x5a00) >> 14;
                        right = 0x7f00 - ((balance * 0x2500) >> 14);
                    }
                    left = (left * volume) >> 15;
                    right = (right * volume) >> 15;
                } else {
                    right = (volume * 0x5a00) >> 15;
                    left = right;
                }
                channel->voice_attr.volume.left = left;
                channel->voice_attr.volume.right = right;
                channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_VOLUME;
            }
            if ((voice_update_flags & SUZUKI_VOICE_UPDATE_PITCH) != 0) {
                channel->voice_attr.pitch = suzuki_smd_calculate_pitch((channel->pitch >> 16)
                                                + channel->pitch_modulation + (music->pitch_shift.value >> 16))
                    & 0x3fff;
                channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_PITCH;
            }
            if ((voice_update_flags & SUZUKI_VOICE_UPDATE_KEY_ON) != 0 && (channel->flags & SUZUKI_CHANNEL_MUTED) == 0)
                key_on |= channel->voice_attr.voice;
            if ((voice_update_flags & SUZUKI_VOICE_UPDATE_KEY_OFF) != 0)
                key_off |= channel->voice_attr.voice;
            channel->voice_update_flags = 0;
        }
        channel++;
    } while (--channel_count != 0);
    music->key_on_mask |= key_on & allowed;
    music->key_off_mask |= key_off & allowed;
}
