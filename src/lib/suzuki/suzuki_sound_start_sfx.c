#include "fft/suzuki.h"
#include "psx/libapi.h"
#include "psx/types.h"

/* Starts SFX sound_id (a SUZUKI_SOUND_ID) on g_suzuki_sound_sfx_channel_count
 * consecutive SFX channels from channel_id & 0xff, one per stream of the
 * sound; the high byte of channel_id is the priority
 * suzuki_sound_find_sfx_channel checks before stealing a channel
 * (SUZUKI_SOUND_SFX_CHANNEL_ID). Channels whose stream offset is 0 are stopped
 * instead. A sound id whose resource is not loaded does nothing. The
 * volume is scaled by the resource's per-sound volume byte, then the SFX
 * voices are claimed in the SFX music and in g_suzuki_sound_sfx_reserved_voices, and
 * music channels on those voices are marked for a full voice update.
 *
 * The loop counter is set at the top of the start branch and flags carries
 * both 0x409 and 0x100: with the for-initializer or two single-set
 * constants, the scheduler orders the branch's first instructions
 * differently from the target. */
void suzuki_sound_start_sfx(s16 channel_id, s32 sound_id, s16 volume, s16 balance) {
    suzuki_sound_resource_t* resource = g_suzuki_sound_resource_list;
    suzuki_waveset_t* waveset;
    suzuki_music_t* music;
    suzuki_music_channel_t* channel;
    u16* offsets;
    s32 count;
    u32 bit;
    u32 started;
    u32 voices = 0;
    u32 stopped;
    u8 priority;
    s32 index;
    s32 i;
    u8* data;
    u16 flags;

    started = 0;
    music = g_suzuki_sound_sfx_music;
    stopped = 0;
    while (resource->id != (sound_id >> 16)) {
        resource = resource->next;
        if (resource == 0) {
            return;
        }
    }
    /* suzuki_sound_find_waveset(music->waveset_id), falling back to the first
     * loaded waveset. */
    waveset = g_suzuki_sound_waveset_list;
    while (waveset != 0 && waveset->id != music->waveset_id) {
        waveset = waveset->next;
    }
    if (waveset == 0) {
        waveset = g_suzuki_sound_waveset_list;
    }
    volume = (volume * ((u8*)resource + resource->volume_offset)[sound_id & 0xffff]) >> 7;
    /* An unsigned clamp of the 16-bit volume (`andi`, `sltiu`); `volume < 0`
     * compiles to a sign test. */
    if ((u16)volume > 0x7fff) {
        volume = 0x7fff;
    }
    index = channel_id & 0xff;
    priority = channel_id >> 8;
    offsets = &resource->channel_offsets[(sound_id & 0xffff) * 2];
    bit = 1 << index;
    channel = &music->channels[index];
    count = g_suzuki_sound_sfx_channel_count;
    DisableEvent(g_suzuki_sound_root_counter_2_event);
    do {
        channel->sound_id = sound_id;
        channel->start_tick = g_suzuki_sound_tick_count;
        channel->priority = priority;
        if (*offsets != 0) {
            i = SUZUKI_MODULATOR_COUNT - 1;
            started |= bit;
            voices |= channel->voice_attr.voice;
            flags = SUZUKI_CHANNEL_KEYED_OFF | SUZUKI_CHANNEL_SFX | SUZUKI_CHANNEL_IN_USE;
            channel->flags = flags;
            flags = SUZUKI_VOICE_UPDATE_VOLUME;
            channel->voice_update_flags = flags;
            channel->ramp_flags = 0;
            data = (u8*)resource + *offsets;
            channel->octave_base = 0x3c;
            channel->gate_time = 0xf;
            channel->loop_note_data = 0;
            channel->_unknown_024 = 0;
            channel->loop_count = 0;
            channel->_unknown_02a = 0;
            channel->event_ticks = 0;
            channel->length_adjust = 0;
            channel->pitch_offset = 0;
            channel->previous_key = 0;
            channel->repeat_depth = 0xffff;
            channel->velocity = volume;
            channel->volume = 0x7f000000;
            channel->balance = balance;
            channel->portamento_ticks = 0;
            channel->pitch_modulation = 0;
            channel->volume_modulation = 0;
            channel->balance_modulation = 0;
            channel->voice_attr.volmode.left = 0;
            channel->voice_attr.volmode.right = 0;
            channel->note_data_start = data;
            channel->note_data = data;
            for (; i >= 0; i--) {
                channel->modulators[i].flags = 0;
            }
            channel->waveset = waveset;
            if (waveset != 0) {
                suzuki_smd_set_instrument(g_suzuki_sound_sfx_instrument, channel);
            }
        } else {
            if (channel->flags & SUZUKI_CHANNEL_IN_USE) {
                stopped |= channel->voice_attr.voice;
            }
            channel->flags = 0;
        }
        offsets++;
        bit <<= 1;
        channel++;
    } while (--count != 0);

    stopped |= voices | g_suzuki_sound_sfx_restart_voices;
    music->channel_mask = started | (~g_suzuki_sound_sfx_restart_channels & music->channel_mask);
    music->key_off_mask &= ~stopped;
    music->key_on_mask &= ~stopped;
    g_suzuki_sound_sfx_key_off_voices |= stopped;
    g_suzuki_sound_sfx_reserved_voices = voices | (~stopped & g_suzuki_sound_sfx_reserved_voices);
    music->lfo_voice_mask &= ~stopped;
    music->noise_voice_mask &= ~stopped;
    music->reverb_voice_mask &= ~stopped;
    g_suzuki_sound_sfx_restart_channels = 0;
    g_suzuki_sound_sfx_restart_voices = 0;
    g_suzuki_sound_voice_update_requests
        |= SUZUKI_VOICE_MASK_REQUEST_PITCH_LFO | SUZUKI_VOICE_MASK_REQUEST_NOISE | SUZUKI_VOICE_MASK_REQUEST_REVERB;
    music->status |= SUZUKI_MUSIC_STATUS_PLAYING;
    if (voices != 0) {
        for (music = g_suzuki_sound_active_music_list; music != 0; music = music->next) {
            /* The playing bit (15) is the sign. */
            if ((music->status & SUZUKI_MUSIC_STATUS_SMD) && music->status < 0) {
                channel = music->channels;
                count = music->channel_count;
                do {
                    if (channel->flags & SUZUKI_CHANNEL_IN_USE) {
                        if (voices & channel->voice_attr.voice) {
                            channel->voice_attr_flags |= 0x7000 | SUZUKI_VOICE_ATTR_ALL;
                        }
                    }
                    channel++;
                } while (--count != 0);
            }
        }
    }
    EnableEvent(g_suzuki_sound_root_counter_2_event);
}
