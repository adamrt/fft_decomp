#include "fft/main_sound.h"
#include "psx/types.h"

/* Per-tick voice pass run after the SMD interpreter and the modulators.
 * note_flags2 bit 8 rebuilds the SPU voice volume: channel volume plus its
 * modulation (clamped to 0-0x7fff), scaled by velocity and the MUS master
 * volume, then split by balance (channel, modulation and MUS balance shift,
 * clamped to 0-0x7f00) into left/right levels when the driver runs in
 * stereo (g_main_sound_driver_flags bit 8), or both set to 0x5a00/0x8000 of the volume in
 * mono. Bit 9 converts the key (pitch slide, pitch modulation and MUS pitch
 * shift) to an SPU pitch. func_flags bits 0 and 2 hand the new volume and
 * pitch to the root-counter voice update. Bits 0 and 1 queue the channel's
 * voices for key on (unless muted) and key off in the MUS masks, filtered by
 * the voices SFX own unless this is the SFX record (status bit 1).
 *
 * The channel pointer is a copy of the parameter so the count parameter is
 * copied first in the prologue, as in the target. */
void main_smd_update_voices(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count) {
    suzuki_music_channel_t* channel;
    u32 key_on;
    u32 key_off;
    u32 allowed;
    u16 note_flags2;
    s32 volume;
    s32 balance;
    s32 left;
    s32 right;

    channel = channels;
    if ((music->status & 2) == 0)
        allowed = ~(g_main_mask_exclusion | g_main_sound_sfx_key_off_voices);
    else
        allowed = -1;
    key_off = 0;
    key_on = 0;
    do {
        if (channel->active != 0 && channel->voice < 25) {
            note_flags2 = channel->note_flags2;
            if ((note_flags2 & 0x100) != 0) {
                volume = (s16)(channel->volume >> 16) + channel->volume_modulation;
                if (volume > 0x7fff)
                    volume = 0x7fff;
                if (volume < 0)
                    volume = 0;
                volume = (channel->velocity * volume) >> 15;
                volume = ((s16)(music->master_volume.value >> 16) * volume) >> 16;
                balance = channel->balance + channel->balance_modulation + (s16)(music->balance_shift.value >> 16);
                if (balance > 0x7f00)
                    balance = 0x7f00;
                if (balance < 0)
                    balance = 0;
                if ((g_main_sound_driver_flags & 0x100) != 0) {
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
                channel->spu_volume_left = left;
                channel->spu_volume_right = right;
                channel->func_flags |= 1;
            }
            if ((note_flags2 & 0x200) != 0) {
                channel->spu_pitch = main_smd_calculate_pitch((s16)(channel->pitch_slide >> 16)
                                         + channel->pitch_modulation + (s16)(music->pitch_shift.value >> 16))
                    & 0x3fff;
                channel->func_flags |= 4;
            }
            if ((note_flags2 & 1) != 0 && (channel->active & 0x20) == 0)
                key_on |= channel->voice_mask;
            if ((note_flags2 & 2) != 0)
                key_off |= channel->voice_mask;
            channel->note_flags2 = 0;
        }
        channel++;
    } while (--channel_count != 0);
    music->key_on_mask |= key_on & allowed;
    music->key_off_mask |= key_off & allowed;
}
