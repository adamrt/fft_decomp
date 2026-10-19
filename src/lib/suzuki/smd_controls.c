/* Music playback controls, queries and snapshot requests. */

#include "fft/suzuki.h"
#include "psx/libapi.h"
#include "psx/types.h"

u32 suzuki_smd_is_music_playing(suzuki_music_t* music) {
    return (music->status & SUZUKI_MUSIC_STATUS_PLAYING) != 0;
}

/* Sets the music's tempo scale (0x100 = 1.0; 0 means 0x100), immediately
 * when time is 0, otherwise as a ramp over time ticks. */
void suzuki_smd_set_tempo_scale(suzuki_music_t* music, s16 value, s16 time) {
    s32 difference;

    if (value == 0) {
        value = 0x100;
    }
    music->tempo_scale.target = value;
    if (time == 0) {
        music->tempo_scale.value = value << 16;
        music->tempo_scale.count = 0;
        music->scaled_tempo = (music->tempo >> 16) * value;
    } else {
        difference = (value << 16) - music->tempo_scale.value;
        if (difference != 0) {
            music->tempo_scale.step = difference / time;
            music->tempo_scale.count = time;
        }
    }
}

/* Sets the music's master volume, immediately when time is 0, otherwise as
 * a ramp over time ticks. A non-zero volume also resumes music paused by
 * suzuki_smd_pause_music, like suzuki_smd_resume_music. */
void suzuki_smd_set_music_volume(suzuki_music_t* music, s16 volume, s16 time) {
    s32 difference;

    music->master_volume.target = volume << 8;
    if (time == 0) {
        music->master_volume.value = volume << 24;
        music->master_volume.count = 0;
        suzuki_smd_set_voice_update_flags_all_channels(SUZUKI_VOICE_UPDATE_VOLUME, music);
    } else {
        difference = (volume << 16) - (music->master_volume.value >> 8);
        if (difference == 0) {
            return;
        }
        music->master_volume.count = time;
        music->master_volume.step = (difference / time) << 8;
    }
    if ((music->status & SUZUKI_MUSIC_STATUS_PAUSED) && volume != 0) {
        DisableEvent(g_suzuki_sound_root_counter_2_event);
        suzuki_sound_set_reverb_mode(
            music->reverb_mode, music->reverb_depth, music->reverb_delay, music->reverb_feedback);
        suzuki_smd_set_voice_attr_flags_all_channels(music, 0x7000 | SUZUKI_VOICE_ATTR_ALL);
        suzuki_smd_retrigger_held_voices(music);
        music->status = (music->status & ~SUZUKI_MUSIC_STATUS_PAUSED) | SUZUKI_MUSIC_STATUS_PLAYING;
        EnableEvent(g_suzuki_sound_root_counter_2_event);
    }
}

/* Sets the music's pitch shift, immediately when time is 0, otherwise as a ramp
 * over time ticks. */
void suzuki_smd_set_pitch_shift(suzuki_music_t* music, s16 value, s16 time) {
    s32 difference;

    music->pitch_shift.target = value << 8;
    if (time == 0) {
        music->pitch_shift.value = value << 24;
        music->pitch_shift.count = 0;
        suzuki_smd_set_voice_update_flags_all_channels(SUZUKI_VOICE_UPDATE_PITCH, music);
    } else {
        difference = (value << 16) - (music->pitch_shift.value >> 8);
        if (difference != 0) {
            music->pitch_shift.count = time;
            music->pitch_shift.step = (difference / time) << 8;
        }
    }
}

/* Sets the music's balance shift, immediately when time is 0, otherwise as a ramp
 * over time ticks. */
void suzuki_smd_set_balance_shift(suzuki_music_t* music, s16 value, s16 time) {
    s32 difference;

    music->balance_shift.target = value << 8;
    if (time == 0) {
        music->balance_shift.value = value << 24;
        music->balance_shift.count = 0;
        suzuki_smd_set_voice_update_flags_all_channels(SUZUKI_VOICE_UPDATE_VOLUME, music);
    } else {
        difference = (value << 16) - (music->balance_shift.value >> 8);
        if (difference != 0) {
            music->balance_shift.count = time;
            music->balance_shift.step = (difference / time) << 8;
        }
    }
}

/* Mutes the channels whose bit is set in mask (bit n = channel n) and
 * unmutes the others. A newly muted channel's voice is queued for key-off;
 * a newly unmuted one is queued for key-on when it is sounding and
 * SUZUKI_CHANNEL_NO_RETRIGGER is clear. GCC merges those two bit tests into
 * one 32-bit `lw`/`andi` test, as in the target. */
void suzuki_smd_set_channel_mute_mask(suzuki_music_t* music, u32 mask) {
    suzuki_music_channel_t* channel;
    u16 key_off;
    u16 key_on;
    s32 count;

    if (music != 0) {
        key_off = 0;
        key_on = 0;
        channel = music->channels;
        count = music->channel_count;
        music->mute_mask = mask;
        do {
            if (channel->flags != 0) {
                if (mask & 1) {
                    if (!(channel->flags & SUZUKI_CHANNEL_MUTED)) {
                        channel->flags |= SUZUKI_CHANNEL_MUTED;
                        key_off |= channel->voice_attr.voice;
                    }
                } else {
                    if (channel->flags & SUZUKI_CHANNEL_MUTED) {
                        channel->flags &= ~SUZUKI_CHANNEL_MUTED;
                        if ((channel->flags & SUZUKI_CHANNEL_SOUNDING)
                            && !(channel->flags & SUZUKI_CHANNEL_NO_RETRIGGER)) {
                            key_on |= channel->voice_attr.voice;
                        }
                    }
                }
            }
            channel++;
            mask >>= 1;
        } while (--count != 0);
        music->key_on_mask |= key_on;
        music->key_off_mask |= key_off;
    }
}

/* Sets the byte that SMD opcode 0x8D compares with its parameter. */
void suzuki_smd_set_channel_select(suzuki_music_t* music, u8 value) {
    music->loop_select = value;
}

/* Fills time with the elapsed play time of a music record. */
void suzuki_smd_get_play_time(suzuki_music_t* music, suzuki_play_time_t* time) {
    u32 ticks;
    u32 seconds;

    ticks = music->ticks >> 8;
    seconds = ticks / 240;
    time->sequence_ticks = music->sequence_ticks;
    time->fraction = ticks % 240;
    time->seconds = seconds % 60;
    time->minutes = seconds / 60;
}

/* Stores the smallest loop count of the music's active channels (0 when
 * none is active) in min_loop_count and returns its address. */
u16* suzuki_smd_get_min_loop_count(suzuki_music_t* music) {
    suzuki_music_channel_t* channel = music->channels;
    u16* result;
    u16 count = music->channel_count;
    u16 min;

    result = &music->min_loop_count;
    min = 0xffff;
    do {
        if (channel->flags != 0 && channel->loop_count < min) {
            min = channel->loop_count;
        }
        channel++;
    } while (--count != 0);
    if (min == 0xffff) {
        min = 0;
    }
    *result = min;
    return result;
}

/* Returns the file name stored in the SMD data of a queued MUS record, or of
 * the most recently queued record when music is 0; 0 if it is not queued. */
char* suzuki_smd_get_music_filename(suzuki_music_t* music) {
    suzuki_music_t* entry = g_suzuki_sound_active_music_list;

    if (music != 0) {
        while (entry != 0 && entry != music) {
            entry = entry->next;
        }
    }
    if (entry == 0) {
        return 0;
    }
    return (char*)entry->smd + entry->smd->filename_offset;
}

/* Discards (0), saves (1) or restores (2) the snapshot of a MUS record. */
void suzuki_smd_dispatch_snapshot(suzuki_music_t* music, s32 mode) {
    switch (mode) {
    case SUZUKI_SNAPSHOT_DISCARD:
        suzuki_smd_discard_snapshot(music);
        break;
    case SUZUKI_SNAPSHOT_SAVE:
        suzuki_smd_save_snapshot(music);
        break;
    case SUZUKI_SNAPSHOT_RESTORE:
        suzuki_smd_restore_snapshot(music);
        break;
    }
}

/* Drop a music record's saved snapshot without restoring it: clear SUZUKI_MUSIC_STATUS_SNAPSHOT
 * and free the snapshot chain. */
void suzuki_smd_discard_snapshot(suzuki_music_t* music) {
    u16 status;

    status = music->status;
    if ((status & SUZUKI_MUSIC_STATUS_SNAPSHOT) != 0) {
        music->status = status & ~SUZUKI_MUSIC_STATUS_SNAPSHOT;
        suzuki_smd_free_snapshots(music);
    }
}

/* Copies the MUS record and its channels into the record's snapshot block,
 * allocating the block on first use, with the root-counter event disabled. */
void suzuki_smd_save_snapshot(suzuki_music_t* music) {
    suzuki_music_t* snapshot;
    u32 size;

    DisableEvent(g_suzuki_sound_root_counter_2_event);
    music->status |= SUZUKI_MUSIC_STATUS_SNAPSHOT;
    size = music->channel_count * sizeof(suzuki_music_channel_t) + sizeof(suzuki_music_t);
    if (music->snapshot == 0) {
        music->snapshot = suzuki_sound_alloc(size);
    }
    snapshot = music->snapshot;
    suzuki_sound_copy_memory(snapshot, music, size);
    snapshot->next = 0;
    snapshot->snapshot = 0;
    music->snapshot_restore_tick = 0;
    EnableEvent(g_suzuki_sound_root_counter_2_event);
}

/* Restores a MUS record from its saved snapshot: queues the voices playing
 * now for key-off, copies the snapshot back, keeps the current sequence_ticks as
 * snapshot_restore_tick, forces a full voice update and retriggers the held notes. */
void suzuki_smd_restore_snapshot(suzuki_music_t* music) {
    s32 tick;

    if (music->snapshot != 0 && (music->status & SUZUKI_MUSIC_STATUS_SNAPSHOT) != 0) {
        DisableEvent(g_suzuki_sound_root_counter_2_event);
        g_suzuki_sound_music_key_off_voices |= suzuki_smd_get_active_voice_mask(music);
        tick = music->sequence_ticks;
        suzuki_smd_copy_snapshot(music, music->snapshot);
        music->snapshot_restore_tick = tick;
        suzuki_smd_set_voice_attr_flags_all_channels(music, 0x7000 | SUZUKI_VOICE_ATTR_ALL);
        music->key_off_mask = 0;
        music->key_on_mask = 0;
        suzuki_smd_retrigger_held_voices(music);
        EnableEvent(g_suzuki_sound_root_counter_2_event);
    }
}

/* Sets the bar at which the root-counter handler stops the music. */
void suzuki_smd_set_stop_bar(suzuki_music_t* music, u16 value) {
    music->stop_bar = value;
}

/* Marks every in-use channel that is sounding and is not muted or flagged
 * SUZUKI_CHANNEL_NO_RETRIGGER for a new key-on and returns the voices of
 * those channels.
 *
 * The two single-bit tests are merged by GCC into one 32-bit
 * `(flags & 0x101) == 0x101` test (`lw`), which the target contains; a
 * direct 0x101 mask test compiles to `lhu`. */
u32 suzuki_smd_retrigger_held_voices(suzuki_music_t* music) {
    suzuki_music_channel_t* channel;
    s32 count = music->channel_count;
    u32 voices;

    channel = music->channels;
    voices = 0;
    do {
        if ((channel->flags & SUZUKI_CHANNEL_IN_USE) && (channel->flags & SUZUKI_CHANNEL_SOUNDING)) {
            if ((channel->flags & (SUZUKI_CHANNEL_MUTED | SUZUKI_CHANNEL_NO_RETRIGGER)) == 0) {
                voices |= channel->voice_attr.voice;
                channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_KEY_ON;
            }
        }
        count--;
        channel++;
    } while (count != 0);
    return voices;
}

/* Returns the voices of every in-use channel that is sounding and is not
 * muted or flagged SUZUKI_CHANNEL_NO_RETRIGGER: the read-only twin of
 * suzuki_smd_retrigger_held_voices, with the same merged bit test. */
u32 suzuki_smd_get_held_voices(suzuki_music_t* music) {
    suzuki_music_channel_t* channel;
    s32 count = music->channel_count;
    u32 voices;

    channel = music->channels;
    voices = 0;
    do {
        if ((channel->flags & SUZUKI_CHANNEL_IN_USE) && (channel->flags & SUZUKI_CHANNEL_SOUNDING)) {
            if ((channel->flags & (SUZUKI_CHANNEL_MUTED | SUZUKI_CHANNEL_NO_RETRIGGER)) == 0) {
                voices |= channel->voice_attr.voice;
            }
        }
        count--;
        channel++;
    } while (count != 0);
    return voices;
}

/* Returns the SPU voices owned by the channels of music that are in use. */
u32 suzuki_smd_get_active_voice_mask(suzuki_music_t* music) {
    suzuki_music_channel_t* channel;
    s32 remaining = music->channel_count;
    u32 active;

    channel = music->channels;
    active = 0;
    do {
        if (channel->flags != 0) {
            active |= channel->voice_attr.voice;
        }
        remaining--;
        channel++;
    } while (remaining != 0);
    return active;
}

/* Empty driver routine; nothing in the tree calls it or takes its address. */
void suzuki_smd_noop_7(void) {
}
