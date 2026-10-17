/* Music and channel initialization, snapshot copies and active music lists. */

#include "fft/suzuki.h"
#include "psx/libapi.h"
#include "psx/libspu.h"
#include "psx/types.h"

/* Allocates the SFX music record (0xbb8 bytes: the header and eight
 * channels), assigns channel n to SPU voice 16 + n and queues the record. */
suzuki_music_t* suzuki_sound_init_sfx_music(void) {
    suzuki_music_t* music
        = suzuki_sound_alloc(sizeof(suzuki_music_t) + SUZUKI_SFX_CHANNEL_COUNT * sizeof(suzuki_music_channel_t));
    suzuki_music_channel_t* channel;
    s32 voice;
    s32 index;
    s32 count;

    suzuki_smd_init_sfx_music_header(music);
    channel = music->channels;
    voice = SUZUKI_SFX_VOICE_FIRST;
    count = SUZUKI_SFX_CHANNEL_COUNT;
    index = 0;
    do {
        channel->flags = 0;
        channel->channel_number = index++;
        channel->voice = voice;
        channel->voice_attr.voice = SPU_VOICECH(voice);
        channel++;
        voice++;
    } while (--count != 0);
    suzuki_smd_insert_music(music);
    return music;
}

/* Copies the SMD header settings into a MUS record, applies its reverb and
 * resets the playback header. */
void suzuki_smd_transfer_music_data(suzuki_music_t* music) {
    suzuki_smd_header_t* smd;

    music->status |= SUZUKI_MUSIC_STATUS_SMD;
    smd = music->smd;
    music->id = smd->id;
    music->_unknown_014 = smd->_unknown_12;
    music->tick_divisor = smd->tick_divisor;
    music->channel_count = smd->channel_count;
    music->_unknown_017 = smd->_unknown_15;
    music->waveset_id = smd->waveset_id;
    music->_unknown_01a = smd->_unknown_18;
    music->reverb_mode = smd->reverb_mode;
    music->reverb_depth = smd->reverb_depth << 8;
    music->reverb_delay = smd->reverb_delay;
    music->reverb_feedback = smd->reverb_feedback;
    suzuki_sound_set_reverb_mode(music->reverb_mode, music->reverb_depth, music->reverb_delay, music->reverb_feedback);
    suzuki_smd_init_music_header(music);
}

/* Sets the SFX music record's fixed header (SUZUKI_MUSIC_STATUS_SFX, id
 * 0x7fff, eight channels, _unknown_01a 0x7f), then the common header
 * defaults. */
void suzuki_smd_init_sfx_music_header(suzuki_music_t* music) {
    music->status = SUZUKI_MUSIC_STATUS_SFX;
    music->id = 0x7fff;
    music->_unknown_014 = 1;
    music->tick_divisor = 1;
    music->channel_count = SUZUKI_SFX_CHANNEL_COUNT;
    music->_unknown_017 = 0;
    music->waveset_id = 0;
    music->_unknown_01a = 0x7f;
    suzuki_smd_init_music_header(music);
}

/* Resets the playback state of a MUS record: frees its snapshots, clears the
 * counters, voice masks and ramps, and sets 4/4 time, tempo 0x66 and full
 * master volume. */
void suzuki_smd_init_music_header(suzuki_music_t* music) {
    suzuki_smd_free_snapshots(music);
    music->_unknown_01c = 0;
    music->loop_select = 0;
    music->min_loop_count = 0;
    music->bar = 1;
    music->beat = 0;
    music->beat_ticks_remaining = 1;
    music->ticks = 0;
    music->sequence_ticks = 0;
    music->timer_ticks = 0;
    music->channel_mask = 0;
    music->key_on_mask = 0;
    music->key_off_mask = 0;
    music->lfo_voice_mask = 0;
    music->noise_voice_mask = 0;
    music->reverb_voice_mask = 0;
    music->pitch_shift.value = 0;
    music->balance_shift.value = 0;
    music->tempo_scale.count = 0;
    music->master_volume.count = 0;
    music->pitch_shift.count = 0;
    music->balance_shift.count = 0;
    music->tempo_step = 0;
    music->tempo_step_count = 0;
    music->beats_per_bar = 4;
    music->beat_unit = 4;
    music->beats_per_bar_copy = 4;
    music->tempo_scale.value = 0x1000000;
    music->master_volume.value = 0x7f000000;
    music->tempo = 0x660000;
    music->scaled_tempo = 0x6600;
    music->tempo_accumulator = 0x10000;
    music->ticks_per_beat = 0x30 / music->tick_divisor;
}

/* Initializes the channels of a MUS record from its SMD channel table.
 *
 * Every channel with note data gets default note flags (muted when its bit
 * is in mute_mask), the SMD id, octave 5, velocity 0x6000, full volume and
 * centered balance, a stream at its SMD offset, instrument 0 of the MUS's
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

/* Detaches the snapshot chain of a MUS record and frees every block in it.
 *
 * It follows the snapshot links (+0x04), not the queue; the queue removal is
 * suzuki_smd_remove_music. The target keeps music and the next link in one
 * register, so music is reused for the link. */
void suzuki_smd_free_snapshots(suzuki_music_t* music) {
    suzuki_music_t* snapshot;

    snapshot = music->snapshot;
    if (snapshot != 0) {
        music->snapshot = 0;
        do {
            music = snapshot->snapshot;
            suzuki_sound_free(snapshot);
            snapshot = music;
        } while (music != 0);
    }
}

/* Copies a saved snapshot over a MUS record and its channels, keeping the
 * record's own queue link and snapshot chain. */
void suzuki_smd_copy_snapshot(suzuki_music_t* music, suzuki_music_t* snapshot) {
    suzuki_music_t* next;
    suzuki_music_t* chain;

    next = music->next;
    chain = music->snapshot;
    suzuki_sound_copy_memory(
        music, snapshot, music->channel_count * sizeof(suzuki_music_channel_t) + sizeof(suzuki_music_t));
    music->next = next;
    music->snapshot = chain;
}

/* Pushes a MUS record onto the head of the active music list with the
 * root-counter event disabled. */
void suzuki_smd_insert_music(suzuki_music_t* music) {
    DisableEvent(g_suzuki_sound_root_counter_2_event);
    music->next = g_suzuki_sound_active_music_list;
    g_suzuki_sound_active_music_list = music;
    EnableEvent(g_suzuki_sound_root_counter_2_event);
}

/* Unlinks a MUS record from the active music list. A playing record is
 * stopped first and its voices are queued for key-off. Returns -1 when the
 * record is not queued, otherwise 0.
 *
 * The target reads status with `lh` and tests the record pointer only after
 * that read. */
s32 suzuki_smd_remove_music(suzuki_music_t* music) {
    suzuki_music_t* entry;
    suzuki_music_t* previous = 0;
    s16 status;

    entry = g_suzuki_sound_active_music_list;
    while (entry != 0) {
        if (entry == music) {
            break;
        }
        previous = entry;
        entry = entry->next;
    }
    if (entry == 0) {
        return -1;
    }
    status = music->status;
    if ((status & SUZUKI_MUSIC_STATUS_PLAYING) && music != 0) {
        music->status = status & ~SUZUKI_MUSIC_STATUS_PLAYING;
        g_suzuki_sound_music_key_off_voices |= suzuki_smd_get_active_voice_mask(music);
    }
    if (previous != 0) {
        previous->next = music->next;
    } else {
        g_suzuki_sound_active_music_list = music->next;
    }
    return 0;
}

/* ORs a mask of suzuki_channel_flag_e bits into the flags of every channel
 * that is in use. No caller is known. */
void suzuki_smd_set_flags_all_channels(u16 mask, suzuki_music_t* music) {
    suzuki_music_channel_t* channel = music->channels;
    s32 count = music->channel_count;

    do {
        if (channel->flags != 0)
            channel->flags |= mask;
        channel++;
    } while (--count != 0);
}

/* Fills the sample address and envelope of an SpuVoiceAttr from an
 * instrument of the first loaded waveset (the list head). addr is the
 * instrument's offset within the waveset, not an absolute SPU address. */
void suzuki_smd_get_instrument_attr(SpuVoiceAttr* attr, s16 index) {
    suzuki_instrument_t* instrument = &g_suzuki_sound_waveset_list->instruments[index];

    attr->addr = instrument->start;
    attr->a_mode = instrument->attack_mode;
    attr->s_mode = instrument->sustain_mode;
    attr->r_mode = instrument->release_mode;
    attr->ar = instrument->attack_rate;
    attr->dr = instrument->decay_rate;
    attr->sr = instrument->sustain_rate;
    attr->rr = instrument->release_rate;
    attr->sl = instrument->sustain_level;
}
