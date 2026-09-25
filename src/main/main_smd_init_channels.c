#include "fft/main.h"
#include "psx/types.h"

/* Initialises the channels of a MUS record from its SMD channel table.
 *
 * Every channel with note data gets default note flags (muted when its bit
 * is in mute_mask), the SMD id, octave 5, velocity 0x6000, full volume and
 * centred balance, a stream at its SMD offset, instrument 0 of the MUS's
 * waveset (the first loaded waveset when that id is not loaded) and an
 * SPU voice: channel n uses voice n - 1 modulo 25, so the first channel gets
 * voice 0xff and no voice bit. music->channel_mask collects one bit per
 * channel index. Channels without note data are cleared.
 *
 * The target compares the voice byte through a u8 copy; the smd reads sit
 * between the stores in source order because the scheduler keeps them
 * ordered against the channel stores. */
void main_smd_init_channels(suzuki_music_t* music) {
    suzuki_music_channel_t* channel;
    suzuki_smd_header_t* smd;
    u16* offsets;
    suzuki_waveset_t* waveset;
    s32 count;
    u32 bit;
    u32 channels;
    s32 voice;
    u8 index;
    s32 i;
    u8* note_data;
    u8 slot;

    count = music->channel_count;
    channel = music->channels;
    if (count != 0) {
        index = 0xff;
        voice = -1;
        channels = 0;
        bit = 1;
        smd = music->smd;
        offsets = smd->channel_offsets;
        waveset = g_main_sound_waveset_list;
        while (waveset != 0) {
            if (waveset->id == music->waveset_id) {
                break;
            }
            waveset = waveset->next;
        }
        if (waveset == 0) {
            waveset = g_main_sound_waveset_list;
        }
        do {
            if (*offsets != 0) {
                channels |= bit;
                if (music->mute_mask & bit) {
                    channel->active = 0x421;
                } else {
                    channel->active = 0x401;
                }
                if (music->status & 0x2000) {
                    channel->active |= 4;
                }
                channel->note_flags2 = 0x300;
                channel->flags_06 = 0;
                channel->sound_id.raw = smd->id;
                channel->priority = 0x10;
                channel->channel_number = index;
                note_data = (u8*)smd + *offsets;
                channel->octave_base = 0x3c;
                channel->gate_time = 0xf;
                channel->repeat_depth = 0xffff;
                channel->velocity = 0x6000;
                channel->volume = 0x7f000000;
                channel->balance = 0x4000;
                slot = voice;
                channel->voice = slot;
                channel->loop_note_data = 0;
                channel->_unknown_024 = 0;
                channel->loop_count = 0;
                channel->_unknown_02a = 0;
                channel->rest_length = 0;
                channel->length_adjust = 0;
                channel->pitch_offset = 0;
                channel->previous_key = 0;
                channel->portamento_ticks = 0;
                channel->pitch_modulation = 0;
                channel->volume_modulation = 0;
                channel->balance_modulation = 0;
                channel->spu_volume_mode_left = 0;
                channel->spu_volume_mode_right = 0;
                channel->note_data_start = note_data;
                channel->note_data = note_data;
                if (slot < 25) {
                    channel->voice_mask = 1 << voice;
                    channel->_unknown_038 = 0xff9f;
                } else {
                    channel->voice_mask = 0;
                    channel->_unknown_038 = 0;
                }
                for (i = 3; i >= 0; i--) {
                    channel->modulators[i].flags = 0;
                }
                channel->waveset = waveset;
                if (waveset != 0) {
                    main_smd_set_instrument(0, channel);
                }
            } else {
                channel->active = 0;
            }
            offsets++;
            voice++;
            bit <<= 1;
            channel++;
            index++;
            if (voice >= 25) {
                voice = 0;
            }
        } while (--count != 0);
        music->channel_mask = channels;
    }
}
