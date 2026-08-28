#include "fft/main_sound.h"
#include "psx/api.h"
#include "psx/etc.h"
#include "psx/types.h"

/* Starts SFX sound_id (resource id << 16 | sound index) on
 * g_main_sound_sfx_channel_count consecutive SFX channels from channel & 0xff; the high byte of
 * channel is the priority main_sound_find_sfx_voice checks before stealing
 * a channel. Channels whose stream offset is 0 are stopped instead. The
 * volume is scaled by the resource's per-sound volume byte, then the SFX
 * voices are claimed in the SFX music and in g_main_mask_exclusion, and
 * music channels on those voices are marked for a full voice update.
 *
 * The loop counter is set at the top of the start branch and flags carries
 * both 0x409 and 0x100: with the for-initialiser or two single-set
 * constants, the scheduler orders the branch's first instructions
 * differently from the target. */
void main_sound_start_sfx(s16 channel_id, s32 sound_id, s16 volume, s16 balance) {
    main_sound_resource_t* resource;
    suzuki_waveset_t* waveset;
    suzuki_music_t* music;
    suzuki_music_channel_t* channel;
    u16* offsets;
    s32 count;
    u32 bit;
    u32 started;
    u32 voices;
    u32 stopped;
    u8 priority;
    s32 index;
    s32 i;
    u8* data;
    u16 flags;

    resource = g_main_sound_resource_list;
    voices = 0;
    started = 0;
    music = g_main_sound_sfx_music;
    stopped = 0;
    while (resource->id != (sound_id >> 16)) {
        resource = resource->next;
        if (resource == 0) {
            return;
        }
    }
    /* main_sound_find_waveset(music->waveset_id), falling back to the first
     * loaded waveset. */
    waveset = g_main_sound_waveset_list;
    while (waveset != 0 && waveset->id != music->waveset_id) {
        waveset = waveset->next;
    }
    if (waveset == 0) {
        waveset = g_main_sound_waveset_list;
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
    count = g_main_sound_sfx_channel_count;
    DisableEvent(g_main_root_counter_2_event);
    do {
        channel->sound_id.raw = sound_id;
        channel->start_tick = g_main_sound_tick_count;
        channel->priority = priority;
        if (*offsets != 0) {
            i = 3;
            started |= bit;
            voices |= channel->voice_mask;
            flags = 0x409;
            channel->active = flags;
            flags = 0x100;
            channel->note_flags2 = flags;
            channel->flags_06 = 0;
            data = (u8*)resource + *offsets;
            channel->octave_base = 0x3c;
            channel->gate_time = 0xf;
            channel->loop_note_data = 0;
            channel->field_24 = 0;
            channel->loop_count = 0;
            channel->field_2a = 0;
            channel->rest_length = 0;
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
            channel->spu_volume_mode_left = 0;
            channel->spu_volume_mode_right = 0;
            channel->note_data_start = data;
            channel->note_data = data;
            for (; i >= 0; i--) {
                channel->modulators[i].flags = 0;
            }
            channel->waveset = waveset;
            if (waveset != 0) {
                main_smd_set_instrument(g_main_sound_sfx_instrument, channel);
            }
        } else {
            if (channel->active & 1) {
                stopped |= channel->voice_mask;
            }
            channel->active = 0;
        }
        offsets++;
        bit <<= 1;
        channel++;
    } while (--count != 0);

    stopped |= voices | g_main_sound_sfx_restart_voices;
    music->channel_mask = started | (~g_main_sound_sfx_restart_channels & music->channel_mask);
    music->key_off_mask &= ~stopped;
    music->key_on_mask &= ~stopped;
    g_main_sound_sfx_key_off_voices |= stopped;
    g_main_mask_exclusion = voices | (~stopped & g_main_mask_exclusion);
    music->lfo_voice_mask &= ~stopped;
    music->noise_voice_mask &= ~stopped;
    music->reverb_voice_mask &= ~stopped;
    g_main_sound_sfx_restart_channels = 0;
    g_main_sound_sfx_restart_voices = 0;
    g_main_sound_voice_update_requests |= 7;
    music->status |= 0x8000;
    if (voices != 0) {
        for (music = g_main_sound_active_music_list; music != 0; music = music->next) {
            /* The playing bit is tested as a sign (`lh`), as elsewhere in the driver. */
            if ((music->status & 1) && (s16)music->status < 0) {
                channel = music->channels;
                count = music->channel_count;
                do {
                    if (channel->active & 1) {
                        if (voices & channel->voice_mask) {
                            channel->func_flags |= 0x71ff;
                        }
                    }
                    channel++;
                } while (--count != 0);
            }
        }
    }
    EnableEvent(g_main_root_counter_2_event);
}
