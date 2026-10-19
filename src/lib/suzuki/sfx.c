/* Sound-effect channel selection, playback and volume control. */

#include "fft/suzuki.h"
#include "psx/libapi.h"
#include "psx/types.h"

/* Ordinary driver-status view; the transfer wait keeps its measured volatile view. */
extern s16 g_suzuki_sound_driver_flags;

/* Sets or clears SUZUKI_SOUND_DRIVER_SFX_ENABLED; disabling also silences
 * every SFX channel first. The Play Sound entry points test the flag. */
void suzuki_sound_set_sfx_enabled(s32 enabled) {
    if (enabled != 0) {
        g_suzuki_sound_driver_flags |= SUZUKI_SOUND_DRIVER_SFX_ENABLED;
    } else {
        suzuki_sound_stop_all_sfx();
        g_suzuki_sound_driver_flags &= ~SUZUKI_SOUND_DRIVER_SFX_ENABLED;
    }
}

/* Plays a sound on SFX channels 6-7 at the default volume (0x60) and center
 * balance, with a priority suzuki_sound_find_sfx_channel never steals. */
void suzuki_sound_play_direct_sfx(s32 sound) {
    if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_SFX_ENABLED) != 0) {
        g_suzuki_sound_sfx_channel_count = 2;
        suzuki_sound_start_sfx(SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_DIRECT, 6), sound, 0x6000, 0x4000);
    }
}

/* Plays a sound on SFX channels 4-5, like suzuki_sound_play_direct_sfx on 6-7. */
void suzuki_sound_play_background_sfx(s32 sound) {
    if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_SFX_ENABLED) != 0) {
        g_suzuki_sound_sfx_channel_count = 2;
        suzuki_sound_start_sfx(
            SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_BACKGROUND, 4), sound, 0x6000, 0x4000);
    }
}

/* Plays a sound effect on a free pair of SFX voices at the default volume
 * (0x60) and center balance (0x40). */
void suzuki_sound_play_sfx(s32 sound_id) {
    if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_SFX_ENABLED) != 0) {
        s32 channel = suzuki_sound_find_sfx_channel(sound_id, 2);
        g_suzuki_sound_sfx_channel_count = 2;
        suzuki_sound_start_sfx(
            SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_STEALABLE, channel), sound_id, 0x6000, 0x4000);
    }
}

/* Plays a sound effect on the SFX voice pair that starts at the even
 * channel at or below channel. */
void suzuki_sound_play_sfx_in_channel(s32 sound_id, s32 channel) {
    u16 first_channel = channel;

    first_channel &= ~1;
    if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_SFX_ENABLED) != 0) {
        g_suzuki_sound_sfx_channel_count = 2;
        suzuki_sound_start_sfx(
            SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_STEALABLE, first_channel), sound_id, 0x6000, 0x4000);
    }
}

/* suzuki_sound_play_sfx with explicit volume and balance bytes; SMD
 * opcode 0x9C (suzuki_smd_play_sfx) passes balance 0x40 (center). */
void suzuki_sound_play_sfx_with_settings_find_channel(s32 sound_id, s32 volume, s32 balance) {
    if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_SFX_ENABLED) != 0) {
        s32 channel = suzuki_sound_find_sfx_channel(sound_id, 2);
        g_suzuki_sound_sfx_channel_count = 2;
        suzuki_sound_start_sfx(SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_STEALABLE, channel), sound_id,
            volume << 8, balance << 8);
    }
}

/* suzuki_sound_play_sfx_in_channel with explicit volume and balance bytes. */
void suzuki_sound_play_sfx_with_settings_in_channel(s32 sound_id, s32 channel, s32 volume, s32 balance) {
    u16 first_channel = channel;

    first_channel &= ~1;
    if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_SFX_ENABLED) != 0) {
        g_suzuki_sound_sfx_channel_count = 2;
        suzuki_sound_start_sfx(SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_STEALABLE, first_channel),
            sound_id, volume << 8, balance << 8);
    }
}

/* Plays two sound effects on the SFX voice pairs 0 and 2. */
void suzuki_sound_play_2_sfx(s32 first, s32 second) {
    if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_SFX_ENABLED) != 0) {
        g_suzuki_sound_sfx_channel_count = 2;
        suzuki_sound_start_sfx(
            SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_STEALABLE, 0), first, 0x6000, 0x4000);
        suzuki_sound_start_sfx(
            SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_STEALABLE, 2), second, 0x6000, 0x4000);
    }
}

/* Plays four sound effects on the SFX voice pairs 0, 2, 4 and 6. */
void suzuki_sound_play_4_sfx(s32 first, s32 second, s32 third, s32 fourth) {
    if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_SFX_ENABLED) != 0) {
        g_suzuki_sound_sfx_channel_count = 2;
        suzuki_sound_start_sfx(
            SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_STEALABLE, 0), first, 0x6000, 0x4000);
        suzuki_sound_start_sfx(
            SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_STEALABLE, 2), second, 0x6000, 0x4000);
        suzuki_sound_start_sfx(
            SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_STEALABLE, 4), third, 0x6000, 0x4000);
        suzuki_sound_start_sfx(
            SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_STEALABLE, 6), fourth, 0x6000, 0x4000);
    }
}

/* Silences the eight channels of the SFX MUS record and queues their voices
 * for key-off. It leaves the music records alone. */
void suzuki_sound_stop_all_sfx(void) {
    suzuki_music_t* music;
    suzuki_music_channel_t* channel;
    u32 voices = 0;
    s32 i;

    music = g_suzuki_sound_sfx_music;
    channel = music->channels;
    for (i = SUZUKI_SFX_CHANNEL_COUNT; i != 0; i--) {
        if (channel->flags & SUZUKI_CHANNEL_IN_USE) {
            channel->flags = 0;
            voices |= channel->voice_attr.voice;
        }
        channel++;
    }
    DisableEvent(g_suzuki_sound_root_counter_2_event);
    music->channel_mask = 0;
    music->key_on_mask = 0;
    music->lfo_voice_mask = 0;
    music->noise_voice_mask = 0;
    music->reverb_voice_mask = 0;
    g_suzuki_sound_sfx_reserved_voices = 0;
    g_suzuki_sound_sfx_key_off_voices |= voices;
    EnableEvent(g_suzuki_sound_root_counter_2_event);
}

/* Stops every SFX channel playing a sound of the given sound resource
 * (sound id high half == resource id). suzuki_sound_remove_resource calls
 * it before unlinking the resource. */
void suzuki_sound_stop_resource_sfx(suzuki_sound_resource_t* resource) {
    suzuki_music_channel_t* channel = g_suzuki_sound_sfx_music->channels;
    s32 sound_id;
    s16 id;
    s32 i;

    id = resource->id;
    for (i = SUZUKI_SFX_CHANNEL_COUNT; i != 0; i--) {
        if (channel->flags & SUZUKI_CHANNEL_IN_USE) {
            sound_id = channel->sound_id;
            if ((sound_id >> 16) == id) {
                suzuki_sound_stop_sfx_channels(sound_id);
            }
        }
        channel++;
    }
}

/* Stops every SFX channel playing sound_id and releases their voices. */
void suzuki_sound_stop_sfx_channels(s32 sound_id) {
    suzuki_music_t* music;
    suzuki_music_channel_t* channel;
    u32 bit = 1;
    u32 channels;
    u32 voices = 0;
    s32 i;

    channels = 0;
    music = g_suzuki_sound_sfx_music;
    channel = music->channels;
    for (i = SUZUKI_SFX_CHANNEL_COUNT; i != 0; i--) {
        if ((channel->flags & SUZUKI_CHANNEL_IN_USE) && channel->sound_id == sound_id) {
            channels |= bit;
            voices |= channel->voice_attr.voice;
            channel->flags = 0;
        }
        bit <<= 1;
        channel++;
    }
    if (channels != 0) {
        suzuki_sound_release_sfx_voices(music, channels, voices);
    }
}

/* Stops SFX channels index and index + 1 if they are in use and queues
 * their voices for key-off. No caller is known. */
void suzuki_sound_stop_sfx_channel_pair(s32 index) {
    suzuki_music_t* music;
    suzuki_music_channel_t* channel;
    u32 bit = 1 << index;
    u32 channels;
    u32 voices = 0;
    s32 count;

    channels = 0;
    count = 2;
    music = g_suzuki_sound_sfx_music;
    channel = &music->channels[index];
    do {
        if (channel->flags & SUZUKI_CHANNEL_IN_USE) {
            channels |= bit;
            channel->flags = 0;
            voices |= channel->voice_attr.voice;
        }
        bit <<= 1;
        channel++;
    } while (--count != 0);
    if (channels != 0) {
        suzuki_sound_release_sfx_voices(music, channels, voices);
    }
}

/* Removes stopped SFX channels from the SFX music's channel mask and their
 * voices from the key-on, pitch-LFO, noise, reverb and reserved-voice masks,
 * queueing the voices for key-off.
 *
 * Both masks are updated in place (`channel_mask &= ...`, `voice_mask |=
 * ...`); separate temporaries allocate differently from the target. */
void suzuki_sound_release_sfx_voices(suzuki_music_t* music, u32 channel_mask, u32 voice_mask) {
    u32 keep_voices;

    channel_mask = ~channel_mask;
    DisableEvent(g_suzuki_sound_root_counter_2_event);
    keep_voices = ~voice_mask;
    channel_mask &= music->channel_mask;
    music->channel_mask = channel_mask;
    voice_mask |= g_suzuki_sound_sfx_key_off_voices;
    g_suzuki_sound_sfx_key_off_voices = voice_mask;
    music->key_on_mask &= keep_voices;
    g_suzuki_sound_sfx_reserved_voices &= keep_voices;
    music->lfo_voice_mask &= keep_voices;
    music->noise_voice_mask &= keep_voices;
    music->reverb_voice_mask &= keep_voices;
    EnableEvent(g_suzuki_sound_root_counter_2_event);
}

/* Sets the velocity of every SFX channel playing sound_id, or stops those
 * channels when volume is 0.
 *
 * The stop branch repeats suzuki_sound_stop_sfx_channels with its own locals
 * and its own read of g_suzuki_sound_sfx_music, which is what the target's
 * register allocation reflects. */
void suzuki_sound_set_sfx_volume(int sound_id, s16 volume) {
    suzuki_music_channel_t* channel;
    s32 count = SUZUKI_SFX_CHANNEL_COUNT;

    channel = g_suzuki_sound_sfx_music->channels;
    if (volume != 0) {
        do {
            if ((channel->flags & SUZUKI_CHANNEL_IN_USE) && channel->sound_id == sound_id) {
                channel->velocity = volume << 8;
                channel->voice_update_flags = SUZUKI_VOICE_UPDATE_VOLUME;
            }
            channel++;
        } while (--count != 0);
    } else {
        suzuki_music_t* sfx;
        suzuki_music_channel_t* stop;
        u32 bit;
        u32 channels;
        u32 voices;
        s32 remaining;

        sfx = g_suzuki_sound_sfx_music;
        stop = sfx->channels;
        bit = 1;
        voices = 0;
        channels = 0;
        remaining = SUZUKI_SFX_CHANNEL_COUNT;
        do {
            if ((stop->flags & SUZUKI_CHANNEL_IN_USE) && stop->sound_id == sound_id) {
                channels |= bit;
                voices |= stop->voice_attr.voice;
                stop->flags = 0;
            }
            bit <<= 1;
            stop++;
        } while (--remaining != 0);
        if (channels != 0) {
            suzuki_sound_release_sfx_voices(sfx, channels, voices);
        }
    }
}

/* Sets the balance of every SFX channel playing sound_id. No caller is
 * known. */
void suzuki_sound_set_sfx_balance(s32 sound_id, s32 balance) {
    suzuki_music_channel_t* channel;
    s32 count = SUZUKI_SFX_CHANNEL_COUNT;

    channel = g_suzuki_sound_sfx_music->channels;
    do {
        if ((channel->flags & SUZUKI_CHANNEL_IN_USE) && channel->sound_id == sound_id) {
            channel->balance = balance << 8;
            channel->voice_update_flags = SUZUKI_VOICE_UPDATE_VOLUME;
        }
        channel++;
    } while (--count != 0);
}

/* Returns the mask of SFX channels playing sound_id, or of every active SFX
 * channel when sound_id is -1. */
u32 suzuki_sound_get_sfx_channels(s32 sound_id) {
    suzuki_music_channel_t* channel;
    u32 bit;
    u32 channels;
    s32 count = SUZUKI_SFX_CHANNEL_COUNT;

    bit = 1;
    channel = g_suzuki_sound_sfx_music->channels;
    channels = 0;
    if (sound_id == -1) {
        do {
            if (channel->flags & SUZUKI_CHANNEL_IN_USE) {
                channels |= bit;
            }
            channel++;
            bit <<= 1;
        } while (--count != 0);
    } else {
        do {
            if ((channel->flags & SUZUKI_CHANNEL_IN_USE) && channel->sound_id == sound_id) {
                channels |= bit;
            }
            channel++;
            bit <<= 1;
        } while (--count != 0);
    }
    return channels;
}

/* Stops the SFX channels already playing sound_id, recording their channel
 * and voice masks in g_suzuki_sound_sfx_restart_channels/g_suzuki_sound_sfx_restart_voices for suzuki_sound_start_sfx,
 * then returns the first channel of a free group of voice_count channels, searching down from channel 6 - voice_count
 * in steps of voice_count. When every group is busy it returns the channel with the smallest start tick among those
 * whose priority is at most STEALABLE; best is left unset if none qualifies, as in the target.
 *
 * mask is reused for the search mask: a separate variable changes the
 * register allocation of the stop loop. */
s32 suzuki_sound_find_sfx_channel(s32 sound_id, s32 voice_count) {
    suzuki_music_t* music = g_suzuki_sound_sfx_music;
    suzuki_music_channel_t* channel;
    u32 mask;
    u32 channels;
    u32 voices = 0;
    s32 count;
    s32 index;
    u32 group;
    u32 busy;
    u32 oldest;
    s32 best;

    channels = 0;
    mask = 1;
    count = SUZUKI_SFX_CHANNEL_COUNT;
    channel = music->channels;
    do {
        if ((channel->flags & SUZUKI_CHANNEL_IN_USE) && channel->sound_id == sound_id) {
            channels |= mask;
            voices |= channel->voice_attr.voice;
            channel->flags = 0;
        }
        mask <<= 1;
        channel++;
    } while (--count != 0);

    index = 6 - voice_count;
    group = 0xFFFFFFFF >> (32 - voice_count);
    mask = group << index;
    music = g_suzuki_sound_sfx_music;
    g_suzuki_sound_sfx_restart_channels = channels;
    g_suzuki_sound_sfx_restart_voices = voices;
    channel = &music->channels[index];
    busy = ~channels & music->channel_mask;
    oldest = -1;
    while (busy & mask) {
        if (channel->start_tick < oldest) {
            if (channel->priority <= SUZUKI_SOUND_SFX_PRIORITY_STEALABLE) {
                oldest = channel->start_tick;
                best = index;
            }
        }
        mask >>= voice_count;
        if (mask < group) {
            index = best;
            break;
        }
        channel -= voice_count;
        index -= voice_count;
    }
    return index;
}
