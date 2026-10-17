/* Sequencer timer updates, SPU voice attributes, key transitions and channel ramps. */

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

/* Voices reserved or awaiting release by sound effects are allowed only for
 * the SFX music record. The root-counter handler contains the same loop. */
void suzuki_sound_flush_key_on(void) {
    suzuki_music_t* music;
    s16 status;
    u32 allowed;
    u32 voices;

    voices = 0;
    allowed = ~(g_suzuki_sound_sfx_reserved_voices | g_suzuki_sound_sfx_key_off_voices);
    music = g_suzuki_sound_active_music_list;
    while (music != 0) {
        status = music->status;
        if (status < 0) {
            if ((status & SUZUKI_MUSIC_STATUS_SFX) != 0)
                voices |= music->key_on_mask;
            else
                voices |= allowed & music->key_on_mask;
            music->key_on_mask = 0;
        }
        music = music->next;
    }

    if (voices != 0)
        SpuSetKey(SPU_ON, voices);
}

/* Apply the same SFX reservation filter as key-on. Stopped music and sound
 * effects awaiting release receive a fast envelope release before key-off.
 * The root-counter handler contains the same loop. */
void suzuki_sound_flush_key_off(void) {
    suzuki_music_t* music;
    s16 status;
    u32 allowed;
    u32 voices;
    u32 pending;
    s32 voice;

    allowed = ~(g_suzuki_sound_sfx_reserved_voices | g_suzuki_sound_sfx_key_off_voices);
    voices = (allowed & g_suzuki_sound_music_key_off_voices) | g_suzuki_sound_sfx_key_off_voices;
    music = g_suzuki_sound_active_music_list;
    while (music != 0) {
        status = music->status;
        if (status < 0) {
            if ((status & SUZUKI_MUSIC_STATUS_SFX) != 0)
                voices |= music->key_off_mask;
            else
                voices |= allowed & music->key_off_mask;
            music->key_off_mask = 0;
        }
        music = music->next;
    }

    pending = (allowed & g_suzuki_sound_music_key_off_voices) | g_suzuki_sound_sfx_key_off_voices;
    if (pending != 0) {
        for (voice = 23; voice >= 0; voice--) {
            if ((pending & SPU_VOICECH(voice)) != 0)
                SpuSetVoiceRRAttr(voice, 6, SPU_VOICE_LINEARDecN);
        }
        g_suzuki_sound_sfx_key_off_voices = 0;
        g_suzuki_sound_music_key_off_voices = 0;
    }

    if (voices != 0)
        SpuSetKey(SPU_OFF, voices);
}

/* Event handler of root counter 2 (g_suzuki_sound_root_counter_2_event): commits
 * pending voice attributes and key-ons, steps the driver volume ramps on
 * every second call, then advances each playing MUS record by as many
 * sequencer ticks as its tempo accumulator (tempo_accumulator, less tempo * scale per
 * call, plus 0x10000 per tick) allows, and finally commits the key-offs.
 *
 * bar/beat count bars and beats (beat_ticks_remaining ticks per beat); the record
 * is stopped when its master volume fades to zero or its bar count reaches
 * stop_bar, and stops playing once no channel is running.
 */
s32 suzuki_sound_root_counter_2_handler(void) {
    suzuki_music_t* music;
    u16 requests;
    s16 count; /* the per-channel passes take an s16 count */

    suzuki_sound_update_voice_attrs();
    requests = g_suzuki_sound_voice_update_requests;
    if ((requests & SUZUKI_VOICE_MASK_REQUEST_PITCH_LFO) != 0)
        suzuki_sound_calculate_pitch_lfo_voices();
    if ((requests & SUZUKI_VOICE_MASK_REQUEST_NOISE) != 0)
        suzuki_sound_calculate_noise_voices();
    if ((requests & SUZUKI_VOICE_MASK_REQUEST_REVERB) != 0)
        suzuki_sound_calculate_reverb_voices();
    g_suzuki_sound_voice_update_requests = 0;
    /* The original handler contains the key-on flush directly. */
    {
        suzuki_music_t* music;
        s16 status;
        u32 allowed;
        u32 voices;

        voices = 0;
        allowed = ~(g_suzuki_sound_sfx_reserved_voices | g_suzuki_sound_sfx_key_off_voices);
        music = g_suzuki_sound_active_music_list;
        while (music != 0) {
            status = music->status;
            if (status < 0) {
                if ((status & SUZUKI_MUSIC_STATUS_SFX) != 0)
                    voices |= music->key_on_mask;
                else
                    voices |= allowed & music->key_on_mask;
                music->key_on_mask = 0;
            }
            music = music->next;
        }

        if (voices != 0)
            SpuSetKey(SPU_ON, voices);
    }

    if ((g_suzuki_sound_tick_count++ & 1) != 0) {
        if (g_suzuki_sound_spu_state.music_ramp.count != 0) {
            suzuki_smd_step_ramp(&g_suzuki_sound_spu_state.music_ramp);
            g_suzuki_sound_spu_state.music_volume = g_suzuki_sound_spu_state.music_ramp.value >> 16;
            suzuki_sound_set_vol_balance(
                g_suzuki_sound_spu_state.music_volume, &g_suzuki_sound_spu_state.common.mvol, 0);
            g_suzuki_sound_spu_state.common.mask |= SPU_COMMON_MVOLL | SPU_COMMON_MVOLR;
        }
        if (g_suzuki_sound_spu_state.cd_ramp.count != 0) {
            suzuki_smd_step_ramp(&g_suzuki_sound_spu_state.cd_ramp);
            g_suzuki_sound_spu_state.cd_volume = g_suzuki_sound_spu_state.cd_ramp.value >> 16;
            suzuki_sound_set_vol_balance(
                g_suzuki_sound_spu_state.cd_volume, &g_suzuki_sound_spu_state.common.cd.volume, 0);
            g_suzuki_sound_spu_state.common.mask |= SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR;
        }
        if (g_suzuki_sound_spu_state.common.mask != 0) {
            SpuSetCommonAttr(&g_suzuki_sound_spu_state.common);
            g_suzuki_sound_spu_state.common.mask = 0;
        }
    }

    music = g_suzuki_sound_active_music_list;
    while (music != 0) {
        /* The playing bit (15) is the sign. */
        if (music->status < 0) {
            if (music->snapshot_restore_tick != 0 && music->sequence_ticks >= music->snapshot_restore_tick)
                suzuki_smd_restore_snapshot(music);
            if (music->tempo_scale.count != 0) {
                suzuki_smd_step_ramp(&music->tempo_scale);
                music->scaled_tempo = (music->tempo >> 16) * (music->tempo_scale.value >> 16);
            }
            if (music->master_volume.count != 0) {
                suzuki_smd_step_ramp(&music->master_volume);
                suzuki_smd_set_voice_update_flags_all_channels(SUZUKI_VOICE_UPDATE_VOLUME, music);
            }
            if (music->pitch_shift.count != 0) {
                suzuki_smd_step_ramp(&music->pitch_shift);
                suzuki_smd_set_voice_update_flags_all_channels(SUZUKI_VOICE_UPDATE_PITCH, music);
            }
            if (music->balance_shift.count != 0) {
                suzuki_smd_step_ramp(&music->balance_shift);
                suzuki_smd_set_voice_update_flags_all_channels(SUZUKI_VOICE_UPDATE_VOLUME, music);
            }
            music->timer_ticks++;
            music->ticks += music->tempo_scale.value >> 16;
            music->tempo_accumulator -= music->scaled_tempo;
            while (music->tempo_accumulator < 0) {
                music->tempo_accumulator += 0x10000;
                if (--music->beat_ticks_remaining == 0) {
                    music->beat_ticks_remaining = music->ticks_per_beat;
                    if (++music->beat > music->beats_per_bar) {
                        music->beat = 1;
                        music->bar++;
                    }
                }
                count = music->channel_count;
                if (count != 0) {
                    suzuki_smd_update_channel_ramps(music, music->channels, count);
                    suzuki_smd_read_instructions(music, music->channels, count);
                    suzuki_smd_update_modulators(music, music->channels, count);
                    suzuki_smd_update_voices(music, music->channels, count);
                }
                if (music->channel_mask != 0) {
                    music->sequence_ticks++;
                    if (music->master_volume.value == 0) {
                        suzuki_smd_stop_music(music);
                        music->status |= (SUZUKI_MUSIC_STATUS_PAUSED | SUZUKI_MUSIC_STATUS_FULL_VOICE_UPDATE);
                    }
                    if (music->bar == music->stop_bar)
                        suzuki_smd_stop_music(music);
                } else {
                    music->status &= ~SUZUKI_MUSIC_STATUS_PLAYING;
                    break;
                }
            }
        }
        music = music->next;
    }

    /* Release stopped voices and commit the queued key-offs. */
    {
        suzuki_music_t* music;
        s16 status;
        u32 allowed;
        u32 voices;
        u32 pending;
        s32 voice;

        allowed = ~(g_suzuki_sound_sfx_reserved_voices | g_suzuki_sound_sfx_key_off_voices);
        voices = (allowed & g_suzuki_sound_music_key_off_voices) | g_suzuki_sound_sfx_key_off_voices;
        music = g_suzuki_sound_active_music_list;
        while (music != 0) {
            status = music->status;
            if (status < 0) {
                if ((status & SUZUKI_MUSIC_STATUS_SFX) != 0)
                    voices |= music->key_off_mask;
                else
                    voices |= allowed & music->key_off_mask;
                music->key_off_mask = 0;
            }
            music = music->next;
        }

        pending = (allowed & g_suzuki_sound_music_key_off_voices) | g_suzuki_sound_sfx_key_off_voices;
        if (pending != 0) {
            for (voice = 23; voice >= 0; voice--) {
                if ((pending & SPU_VOICECH(voice)) != 0)
                    SpuSetVoiceRRAttr(voice, 6, SPU_VOICE_LINEARDecN);
            }
            g_suzuki_sound_sfx_key_off_voices = 0;
            g_suzuki_sound_music_key_off_voices = 0;
        }

        if (voices != 0)
            SpuSetKey(SPU_OFF, voices);
    }
    return 0;
}

/* Linear ramp: value slides by step until count expires, then snaps to
 * target. */
void suzuki_smd_step_ramp(suzuki_ramp_t* ramp) {
    if (--ramp->count != 0) {
        ramp->value += ramp->step;
    } else {
        ramp->value = ramp->target << 16;
    }
}

/* Collects the pitch-LFO voice masks of every playing MUS record and hands
 * them to the SPU; SMD music records lose the voices reserved by SFX
 * (g_suzuki_sound_sfx_reserved_voices), the SFX record keeps its own. */
void suzuki_sound_calculate_pitch_lfo_voices(void) {
    suzuki_music_t* music;
    s32 status;
    u32 excludable_voices;
    u32 forced_voices = 0;

    excludable_voices = 0;
    music = g_suzuki_sound_active_music_list;
    while (music != 0) {
        /* The playing bit (15) is the sign. */
        status = music->status;
        if (status < 0) {
            if ((status & SUZUKI_MUSIC_STATUS_SMD) != 0)
                excludable_voices |= music->lfo_voice_mask;
            else
                forced_voices |= music->lfo_voice_mask;
        }
        music = music->next;
    }

    excludable_voices = (excludable_voices & ~g_suzuki_sound_sfx_reserved_voices) | forced_voices;
    SpuSetPitchLFOVoice(SPU_ON, excludable_voices);
    SpuSetPitchLFOVoice(SPU_OFF, ~excludable_voices);
}

/* Collects the noise voice masks of every playing MUS record and hands them
 * to the SPU; SMD music records lose the voices reserved by SFX
 * (g_suzuki_sound_sfx_reserved_voices), the SFX record keeps its own. */
void suzuki_sound_calculate_noise_voices(void) {
    suzuki_music_t* music;
    s32 status;
    u32 excludable_voices;
    u32 forced_voices = 0;

    excludable_voices = 0;
    music = g_suzuki_sound_active_music_list;
    while (music != 0) {
        /* The playing bit (15) is the sign. */
        status = music->status;
        if (status < 0) {
            if ((status & SUZUKI_MUSIC_STATUS_SMD) != 0)
                excludable_voices |= music->noise_voice_mask;
            else
                forced_voices |= music->noise_voice_mask;
        }
        music = music->next;
    }

    excludable_voices = (excludable_voices & ~g_suzuki_sound_sfx_reserved_voices) | forced_voices;
    SpuSetNoiseVoice(SPU_ON, excludable_voices);
    SpuSetNoiseVoice(SPU_OFF, ~excludable_voices);
}

/* Collects the reverb voice masks of every playing MUS record and hands them
 * to the SPU; SMD music records lose the voices reserved by SFX
 * (g_suzuki_sound_sfx_reserved_voices), the SFX record keeps its own. */
void suzuki_sound_calculate_reverb_voices(void) {
    suzuki_music_t* music;
    s32 status;
    u32 excludable_voices;
    u32 forced_voices = 0;

    excludable_voices = 0;
    music = g_suzuki_sound_active_music_list;
    while (music != 0) {
        /* The playing bit (15) is the sign. */
        status = music->status;
        if (status < 0) {
            if ((status & SUZUKI_MUSIC_STATUS_SMD) != 0)
                excludable_voices |= music->reverb_voice_mask;
            else
                forced_voices |= music->reverb_voice_mask;
        }
        music = music->next;
    }

    excludable_voices = (excludable_voices & ~g_suzuki_sound_sfx_reserved_voices) | forced_voices;
    SpuSetReverbVoice(SPU_ON, excludable_voices);
    SpuSetReverbVoice(SPU_OFF, ~excludable_voices);
}

/* Per-tick channel pass run before the SMD interpreter. Steps the MUS tempo
 * ramp (Tempo Ramp) and rescales the tempo, then for every channel still
 * inside a note or rest (event_ticks != 0) clears the modulation outputs,
 * steps the volume, pitch-slide (unless held) and balance ramps that
 * ramp_flags marks, and counts down event_ticks and note_ticks.
 * voice_update_flags collects the resulting voice work: volume/balance,
 * pitch and key off. When event_ticks reaches 1 on a channel whose next
 * event is a note (SUZUKI_CHANNEL_NOTE_FOLLOWS) the release rate is forced
 * to 6, so the voice has faded by the key-on. */
void suzuki_smd_update_channel_ramps(suzuki_music_t* music, suzuki_music_channel_t* channel, s16 channel_count) {
    u16 count = music->tempo_step_count;
    u16 active;
    u16 voice_update_flags;
    u16 flags;

    if (count != 0) {
        if (--count != 0)
            music->tempo += music->tempo_step;
        else
            music->tempo = music->tempo_target << 16;
        music->tempo_step_count = count;
        music->scaled_tempo = (music->tempo >> 16) * (music->tempo_scale.value >> 16);
    }
    do {
        active = channel->flags;
        if (active != 0) {
            voice_update_flags = channel->voice_update_flags;
            flags = channel->ramp_flags;
            if (channel->event_ticks != 0) {
                channel->pitch_modulation = 0;
                channel->volume_modulation = 0;
                channel->balance_modulation = 0;
                if ((flags & SUZUKI_RAMP_VOLUME) != 0) {
                    voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME;
                    if (--channel->volume_step_count == 0)
                        flags &= ~SUZUKI_RAMP_VOLUME;
                    channel->volume += channel->volume_step;
                }
                if ((flags & SUZUKI_RAMP_PITCH_SLIDE) != 0) {
                    voice_update_flags |= SUZUKI_VOICE_UPDATE_PITCH;
                    if ((flags & SUZUKI_RAMP_PITCH_SLIDE_HOLD) == 0 && --channel->pitch_slide_count == 0)
                        flags &= ~SUZUKI_RAMP_PITCH_SLIDE;
                    channel->pitch += channel->pitch_slide_step;
                }
                if ((flags & SUZUKI_RAMP_BALANCE) != 0) {
                    voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME;
                    if (--channel->balance_step_count == 0)
                        flags &= ~SUZUKI_RAMP_BALANCE;
                    channel->balance += channel->balance_step;
                }
                if (--channel->event_ticks == 1 && (active & SUZUKI_CHANNEL_NOTE_FOLLOWS) != 0) {
                    channel->voice_attr.rr = 6;
                    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_RELEASE;
                }
                if ((active & (SUZUKI_CHANNEL_KEYED_OFF | SUZUKI_CHANNEL_TIED)) == 0 && --channel->note_ticks == 0) {
                    voice_update_flags |= SUZUKI_VOICE_UPDATE_KEY_OFF;
                    channel->flags |= SUZUKI_CHANNEL_KEYED_OFF;
                }
            }
            channel->voice_update_flags = voice_update_flags;
            channel->ramp_flags = flags;
        }
        channel++;
    } while (--channel_count != 0);
}
