/* World-map sound effects and the queued music load, fade and playback commands. */

#include "fft/file.h"
#include "fft/music.h"
#include "fft/sound.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* Play sound effect `sound_id` (WLDCORE's wrapper for sound_play_sfx). */
void sound_play_effect(s32 sound_id) {
    sound_play_sfx(sound_id);
}

/* Queue an audio command, servicing frame work while the queue is full.
 *
 * Accessing the count as a g_audio_queue member is what makes GCC reload it
 * after each array store. */
void audio_queue_add(audio_command_e command, s32 value) {
    while (g_audio_queue.count >= AUDIO_QUEUE_CAPACITY) {
        file_poll_load(&g_file_cd_state);
        audio_queue_process();
        VSync(0);
    }

    g_audio_queue.commands[g_audio_queue.count] = command;
    g_audio_queue.values[g_audio_queue.count] = value;
    g_audio_queue.count++;
}

/* Services the audio command FIFO: dequeues the next command when idle and
 * runs the current one until it has to wait for loading or a fade.
 *
 * PLAY_MUSIC loads track `value & 0xff` into slot `value >> 8`, or just
 * switches to it when the slot already holds that track. FADE_OUT fades the
 * music out when it is playing, FADE_IN switches to the current slot and
 * fades in to `volume`, and UNLOAD releases a slot (0: the current one); the
 * WAIT commands wait for the load or count down the fade. The dispatch loop is a
 * backward goto: a `for (;;)` loop lets loop.c hoist the queue addresses
 * into saved registers, which the target never does. The if/else in case 4
 * keeps CSE from carrying the value's address across the join. Cases 2,
 * 0x10 and 0x11 jump into the default arm's reset as the target does; a
 * duplicated reset is cross-jumped into the last copy instead. */
void audio_queue_process(void) {
    s32 i;
    s32 value;
    s32 slot;
    s32 track;
    s32* slot_track;

next:
    switch (g_audio_queue.current_command) {
    case AUDIO_COMMAND_NONE:
        if (g_audio_queue.count == 0) {
            return;
        }
        g_audio_queue.current_command = g_audio_queue.commands[0];
        g_audio_queue.current_value = g_audio_queue.values[0];
        for (i = 0; i < g_audio_queue.count - 1; i++) {
            g_audio_queue.commands[i] = g_audio_queue.commands[i + 1];
            g_audio_queue.values[i] = g_audio_queue.values[i + 1];
        }
        g_audio_queue.count--;
        break;
    case AUDIO_COMMAND_PLAY_MUSIC:
        if (g_file_cd_state.state != 0) {
            return;
        }
        slot = (g_audio_queue.current_value & 0xF00) >> 8;
        track = g_audio_queue.current_value & 0xFF;
        slot_track = &g_audio_queue.music.tracks[slot - 1];
        if (*slot_track == track) {
            g_audio_queue.current_slot = slot;
            g_audio_queue.current_command = AUDIO_COMMAND_NONE;
            music_switch_track(slot, 0, 0);
            return;
        }
        if (*slot_track != 0) {
            music_unload_scenario_slot(slot);
        }
        if (music_load_into_slot(track, slot) != 0) {
            g_audio_queue.current_command = AUDIO_COMMAND_WAIT_LOAD;
            g_audio_queue.current_slot = slot;
            *slot_track = track;
            return;
        }
        g_audio_queue.current_command = AUDIO_COMMAND_NONE;
        return;
    case AUDIO_COMMAND_FADE_OUT:
        if ((g_audio_queue.music.flags & AUDIO_MUSIC_PLAYING) == 0) {
            goto clear_command;
        }
        music_set_target(0, g_audio_queue.current_value << 2);
        g_audio_queue.current_command = AUDIO_COMMAND_WAIT_FADE_OUT;
        return;
    case AUDIO_COMMAND_FADE_IN:
        music_switch_track(g_audio_queue.current_slot, 0, 0);
        music_set_target(g_audio_queue.volume, g_audio_queue.current_value << 2);
        g_audio_queue.current_command = AUDIO_COMMAND_WAIT_FADE_IN;
        return;
    case AUDIO_COMMAND_UNLOAD:
        value = g_audio_queue.current_value;
        if (value != 0) {
            slot = value;
        } else {
            slot = g_audio_queue.current_slot;
        }
        music_unload_scenario_slot(slot);
        if (slot == g_audio_queue.current_slot) {
            g_audio_queue.music.flags &= ~AUDIO_MUSIC_PLAYING;
        }
        g_audio_queue.music.tracks[slot - 1] = 0;
    default:
    clear_command:
        g_audio_queue.current_command = AUDIO_COMMAND_NONE;
        break;
    case AUDIO_COMMAND_WAIT_LOAD:
        if (g_file_cd_state.state == 0) {
            goto clear_command;
        }
        return;
    case AUDIO_COMMAND_WAIT_FADE_OUT:
        if (--g_audio_queue.current_value != 0) {
            return;
        }
        g_audio_queue.music.flags &= ~AUDIO_MUSIC_PLAYING;
        music_stop();
        goto clear_command;
    case AUDIO_COMMAND_WAIT_FADE_IN:
        if (--g_audio_queue.current_value != 0) {
            return;
        }
        g_audio_queue.current_command = AUDIO_COMMAND_NONE;
        g_audio_queue.music.flags |= AUDIO_MUSIC_PLAYING;
        break;
    }

    goto next;
}

/* Queue a music change: fade out what is playing, start `value` (a packed
 * slot and music id) and fade it in over 0x10 steps. */
void audio_queue_start_music(s32 value) {
    if (g_audio_queue.music.flags & AUDIO_MUSIC_PLAYING) {
        audio_queue_add(AUDIO_COMMAND_FADE_OUT, 0x10);
    }
    audio_queue_add(AUDIO_COMMAND_PLAY_MUSIC, value);
    audio_queue_add(AUDIO_COMMAND_FADE_IN, 0x10);
}

/* Twin of file_wait_for_pending with the audio queue as the wait condition:
 * run frames until no command is queued or running. The two 32-bit busy
 * fields are read as g_audio_queue members rather than as separate
 * scalars. */
void audio_queue_wait_until_empty(void) {
    while (g_audio_queue.count != 0 || g_audio_queue.current_command != AUDIO_COMMAND_NONE) {
        file_poll_load(&g_file_cd_state);
        audio_queue_process();
        VSync(0);
    }
}
