/* Create, stop, pause, resume and unload SMD music records. */

#include "fft/suzuki.h"
#include "psx/libapi.h"
#include "psx/types.h"

/* Allocates a MUS record for an SMD sequence, reads its header and channels,
 * and queues it. The returned record is the caller's music handle. */
suzuki_music_t* suzuki_smd_create_music(suzuki_smd_header_t* smd) {
    suzuki_music_t* music
        = suzuki_sound_alloc(smd->channel_count * sizeof(suzuki_music_channel_t) + sizeof(suzuki_music_t));

    music->smd = smd;
    suzuki_smd_transfer_music_data(music);
    suzuki_smd_init_channels(music);
    music->mute_mask = 0;
    suzuki_smd_insert_music(music);
    return music;
}

/* Stops a music handle if it is playing, unlinks it from the queue and,
 * when it was queued, frees its snapshots and the record itself. */
void suzuki_smd_unload_music(suzuki_music_t* music) {
    if (music->status & SUZUKI_MUSIC_STATUS_PLAYING) {
        suzuki_smd_stop_music(music);
    }
    if (suzuki_smd_remove_music(music) == 0) {
        suzuki_smd_free_snapshots(music);
        suzuki_sound_free(music);
    }
}

/* Empty driver routine; nothing in the tree calls it or takes its address. */
void suzuki_smd_noop_1(void) {
}

/* Empty driver routine; nothing in the tree calls it or takes its address. */
void suzuki_smd_noop_2(void) {
}

/* Restarts a music handle from the top of its sequence: re-reads the SMD
 * header and channels with the root-counter event disabled, fades the master
 * volume in from 0 and marks the record playing again. */
void suzuki_smd_reset_music(suzuki_music_t* music, s16 volume, s16 fade) {
    if (music != 0) {
        music->status &= ~SUZUKI_MUSIC_STATUS_PLAYING;
        DisableEvent(g_suzuki_sound_root_counter_2_event);
        suzuki_smd_transfer_music_data(music);
        suzuki_smd_init_channels(music);
        music->master_volume.value = 0;
        suzuki_smd_set_music_volume(music, volume, fade);
        suzuki_smd_set_voice_attr_flags_all_channels(music, 0x7000);
        music->status |= SUZUKI_MUSIC_STATUS_PLAYING;
        EnableEvent(g_suzuki_sound_root_counter_2_event);
    }
}

/* Resumes a music handle paused by suzuki_smd_pause_music: with the
 * root-counter event disabled it fades the master volume in from 0,
 * re-applies the song's reverb mode, forces every channel's voice
 * parameters, retriggers held voices, then clears the pause bit and marks
 * the record playing. */
void suzuki_smd_resume_music(suzuki_music_t* music, s16 volume, s16 fade) {
    if (music != 0) {
        DisableEvent(g_suzuki_sound_root_counter_2_event);
        music->master_volume.value = 0;
        suzuki_smd_set_music_volume(music, volume, fade);
        suzuki_sound_set_reverb_mode(
            music->reverb_mode, music->reverb_depth, music->reverb_delay, music->reverb_feedback);
        suzuki_smd_set_voice_attr_flags_all_channels(music, 0x7000 | SUZUKI_VOICE_ATTR_ALL);
        suzuki_smd_retrigger_held_voices(music);
        music->status = (music->status & ~SUZUKI_MUSIC_STATUS_PAUSED) | SUZUKI_MUSIC_STATUS_PLAYING;
        EnableEvent(g_suzuki_sound_root_counter_2_event);
    }
}

/* Stops a music record: clears its playing bit and queues the voices of its
 * channels for key-off. The record stays queued and loaded. */
void suzuki_smd_stop_music(suzuki_music_t* music) {
    if (music != 0) {
        music->status &= ~SUZUKI_MUSIC_STATUS_PLAYING;
        g_suzuki_sound_music_key_off_voices |= suzuki_smd_get_active_voice_mask(music);
    }
}

/* Pauses a music handle: clears the playing bit, sets the pause bit that
 * suzuki_smd_resume_music clears again, and queues the voices of its active
 * channels for key-off. */
void suzuki_smd_pause_music(suzuki_music_t* music) {
    if (music != 0) {
        music->status = (music->status & ~SUZUKI_MUSIC_STATUS_PLAYING) | SUZUKI_MUSIC_STATUS_PAUSED;
        g_suzuki_sound_music_key_off_voices |= suzuki_smd_get_active_voice_mask(music);
    }
}

/* Stops every queued record that plays SMD music (not the SFX record),
 * queueing the voices of its active channels for key-off. */
void suzuki_smd_stop_marked_music(void) {
    suzuki_music_t* music = g_suzuki_sound_active_music_list;

    while (music != 0) {
        if (music->status & SUZUKI_MUSIC_STATUS_SMD) {
            music->status &= ~SUZUKI_MUSIC_STATUS_PLAYING;
            g_suzuki_sound_music_key_off_voices |= suzuki_smd_get_active_voice_mask(music);
        }
        music = music->next;
    }
}

/* Points each channel's restart stream at its SMD channel data (a zero
 * offset or a channel past the header's count gets no stream) and marks the
 * stream set (SUZUKI_CHANNEL_STREAMS_SET). */
void suzuki_smd_init_channel_streams(suzuki_music_t* music, suzuki_smd_header_t* smd) {
    suzuki_music_channel_t* channel = music->channels;
    u16* offsets = smd->channel_offsets;
    s32 remaining;
    s32 count = music->channel_count;

    remaining = smd->channel_count;
    do {
        channel->flags |= SUZUKI_CHANNEL_STREAMS_SET;
        if (remaining != 0) {
            channel->restart_note_data = *offsets != 0 ? (u8*)smd + *offsets : 0;
            remaining--;
            offsets++;
        } else {
            channel->restart_note_data = 0;
        }
        channel++;
    } while (--count != 0);
}

/* Empty driver routine, the first of four in a row; nothing in the tree calls them or takes their
 * address. */
void suzuki_smd_noop_3(void) {
}

/* Empty driver routine; nothing in the tree calls it or takes its address. */
void suzuki_smd_noop_4(void) {
}

/* Empty driver routine; nothing in the tree calls it or takes its address. */
void suzuki_smd_noop_5(void) {
}

/* Empty driver routine; nothing in the tree calls it or takes its address. */
void suzuki_smd_noop_6(void) {
}
