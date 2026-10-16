#include "psx/suzuki.h"
#include "psx/types.h"

/* Initializes the channels of a MUS record from its SMD channel table.
 *
 * Every channel with note data gets default note flags (muted when its bit
 * is in mute_mask), the SMD id, octave 5, velocity 0x6000, full volume and
 * centerd balance, a stream at its SMD offset, instrument 0 of the MUS's
 * waveset (the first loaded waveset when that id is not loaded) and an
 * SPU voice: channel n uses voice n - 1 modulo 25, so the first channel gets
 * voice 0xff and no voice bit. music->channel_mask collects one bit per
 * channel index. Channels without note data are cleared.
 *
 * The target compares the voice byte through a u8 copy; the smd reads sit
 * between the stores in source order because the scheduler keeps them
 * ordered against the channel stores. */
void suzuki_smd_init_channels(suzuki_music_t* music) {
    suzuki_music_channel_t* channel;
    suzuki_smd_header_t* smd;
    u16* offsets;
    suzuki_waveset_t* waveset;
    s32 count = music->channel_count;
    u32 bit;
    u32 channels;
    s32 voice;
    u8 index;
    s32 i;
    u8* note_data;
    u8 slot;

    channel = music->channels;
    if (count != 0) {
        index = 0xff;
        voice = -1;
        channels = 0;
        bit = 1;
        smd = music->smd;
        offsets = smd->channel_offsets;
        waveset = g_suzuki_sound_waveset_list;
        while (waveset != 0) {
            if (waveset->id == music->waveset_id) {
                break;
            }
            waveset = waveset->next;
        }
        if (waveset == 0) {
            waveset = g_suzuki_sound_waveset_list;
        }
        do {
            if (*offsets != 0) {
                channels |= bit;
                if (music->mute_mask & bit) {
                    channel->flags = SUZUKI_CHANNEL_KEYED_OFF | SUZUKI_CHANNEL_MUTED | SUZUKI_CHANNEL_IN_USE;
                } else {
                    channel->flags = SUZUKI_CHANNEL_KEYED_OFF | SUZUKI_CHANNEL_IN_USE;
                }
                if (music->status & SUZUKI_MUSIC_STATUS_IMMEDIATE_INSTRUMENT) {
                    channel->flags |= SUZUKI_CHANNEL_IMMEDIATE_INSTRUMENT;
                }
                channel->voice_update_flags = SUZUKI_VOICE_UPDATE_VOLUME | SUZUKI_VOICE_UPDATE_PITCH;
                channel->ramp_flags = 0;
                channel->sound_id = smd->id;
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
                channel->event_ticks = 0;
                channel->length_adjust = 0;
                channel->pitch_offset = 0;
                channel->previous_key = 0;
                channel->portamento_ticks = 0;
                channel->pitch_modulation = 0;
                channel->volume_modulation = 0;
                channel->balance_modulation = 0;
                channel->voice_attr.volmode.left = 0;
                channel->voice_attr.volmode.right = 0;
                channel->note_data_start = note_data;
                channel->note_data = note_data;
                if (slot < 25) {
                    channel->voice_attr.voice = SPU_VOICECH(voice);
                    channel->voice_attr.mask = SPU_VOICE_VOLL | SPU_VOICE_VOLR | SPU_VOICE_VOLMODEL | SPU_VOICE_VOLMODER
                        | SPU_VOICE_PITCH | SPU_VOICE_WDSA | SPU_VOICE_ADSR_AMODE | SPU_VOICE_ADSR_SMODE
                        | SPU_VOICE_ADSR_RMODE | SPU_VOICE_ADSR_AR | SPU_VOICE_ADSR_DR | SPU_VOICE_ADSR_SR
                        | SPU_VOICE_ADSR_RR | SPU_VOICE_ADSR_SL;
                } else {
                    channel->voice_attr.voice = 0;
                    channel->voice_attr.mask = 0;
                }
                for (i = SUZUKI_MODULATOR_COUNT - 1; i >= 0; i--) {
                    channel->modulators[i].flags = 0;
                }
                channel->waveset = waveset;
                if (waveset != 0) {
                    suzuki_smd_set_instrument(0, channel);
                }
            } else {
                channel->flags = 0;
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
