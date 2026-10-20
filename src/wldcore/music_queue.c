/* WLDCORE's music queue keeps location changes, Brave Story, Tutorial and
 * proposition screens in step with CD loads and music fades. Level handlers
 * enqueue slot changes; frame polling advances them until the selected track
 * is ready. The leading sound-effect wrapper plays immediately. */

#include "fft/file.h"
#include "fft/music.h"
#include "fft/sound.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* WLDCORE keeps the pending commands and playback state between menu frames. */
music_queue_t g_music_queue = { 0 };

/* Play sound effect `sound_id` (WLDCORE's wrapper for sound_sfx_play). */
void sound_play_effect(s32 sound_id) {
    sound_sfx_play(sound_id);
}

/* Queue a music command, servicing frame work while the queue is full.
 *
 * Accessing the count as a g_music_queue member is what makes GCC reload it
 * after each array store. */
void music_queue_add(music_queue_command_e command, s32 value) {
    while (g_music_queue.count >= MUSIC_QUEUE_CAPACITY) {
        file_load_poll(&g_file_cd_state);
        music_queue_process();
        VSync(0);
    }

    g_music_queue.commands[g_music_queue.count] = command;
    g_music_queue.values[g_music_queue.count] = value;
    g_music_queue.count++;
}

/* Services the music command FIFO: dequeues the next command when idle and
 * runs the current one until it has to wait for loading or a fade.
 *
 * PLAY_TRACK loads track `value & 0xff` into slot `(value >> 8) & 0xf`, or just
 * switches to it when the slot already holds that track. FADE_OUT fades the
 * music out when it is playing, FADE_IN switches to the current slot and
 * fades in to `volume`, and UNLOAD releases a slot (0: the current one); the
 * WAIT commands wait for the load or count down the fade. The dispatch loop is a
 * backward goto: a `for (;;)` loop lets loop.c hoist the queue addresses
 * into saved registers, which the target never does. The if/else in case 4
 * keeps CSE from carrying the value's address across the join. Cases 2,
 * 0x10 and 0x11 jump into the default arm's reset as the target does; a
 * duplicated reset is cross-jumped into the last copy instead. */
void music_queue_process(void) {
    s32 i;
    s32 value;
    s32 slot;
    s32 track;
    s32* slot_track;

next:
    switch (g_music_queue.current_command) {
    case MUSIC_QUEUE_COMMAND_NONE:
        if (g_music_queue.count == 0) {
            return;
        }
        g_music_queue.current_command = g_music_queue.commands[0];
        g_music_queue.current_value = g_music_queue.values[0];
        for (i = 0; i < g_music_queue.count - 1; i++) {
            g_music_queue.commands[i] = g_music_queue.commands[i + 1];
            g_music_queue.values[i] = g_music_queue.values[i + 1];
        }
        g_music_queue.count--;
        break;
    case MUSIC_QUEUE_COMMAND_PLAY_TRACK:
        if (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
            return;
        }
        slot = (g_music_queue.current_value & 0xF00) >> 8;
        track = g_music_queue.current_value & 0xFF;
        slot_track = &g_music_queue.music.tracks[slot - 1];
        if (*slot_track == track) {
            g_music_queue.current_slot = slot;
            g_music_queue.current_command = MUSIC_QUEUE_COMMAND_NONE;
            music_track_switch(slot, 0, 0);
            return;
        }
        if (*slot_track != 0) {
            music_scenario_slot_unload(slot);
        }
        if (music_slot_load(track, slot) != 0) {
            g_music_queue.current_command = MUSIC_QUEUE_COMMAND_WAIT_LOAD;
            g_music_queue.current_slot = slot;
            *slot_track = track;
            return;
        }
        g_music_queue.current_command = MUSIC_QUEUE_COMMAND_NONE;
        return;
    case MUSIC_QUEUE_COMMAND_FADE_OUT:
        if ((g_music_queue.music.flags & MUSIC_QUEUE_PLAYING) == 0) {
            goto clear_command;
        }
        music_volume_target_set(0, g_music_queue.current_value << 2);
        g_music_queue.current_command = MUSIC_QUEUE_COMMAND_WAIT_FADE_OUT;
        return;
    case MUSIC_QUEUE_COMMAND_FADE_IN:
        music_track_switch(g_music_queue.current_slot, 0, 0);
        music_volume_target_set(g_music_queue.volume, g_music_queue.current_value << 2);
        g_music_queue.current_command = MUSIC_QUEUE_COMMAND_WAIT_FADE_IN;
        return;
    case MUSIC_QUEUE_COMMAND_UNLOAD:
        value = g_music_queue.current_value;
        if (value != 0) {
            slot = value;
        } else {
            slot = g_music_queue.current_slot;
        }
        music_scenario_slot_unload(slot);
        if (slot == g_music_queue.current_slot) {
            g_music_queue.music.flags &= ~MUSIC_QUEUE_PLAYING;
        }
        g_music_queue.music.tracks[slot - 1] = 0;
    default:
    clear_command:
        g_music_queue.current_command = MUSIC_QUEUE_COMMAND_NONE;
        break;
    case MUSIC_QUEUE_COMMAND_WAIT_LOAD:
        if (g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
            goto clear_command;
        }
        return;
    case MUSIC_QUEUE_COMMAND_WAIT_FADE_OUT:
        if (--g_music_queue.current_value != 0) {
            return;
        }
        g_music_queue.music.flags &= ~MUSIC_QUEUE_PLAYING;
        music_stop();
        goto clear_command;
    case MUSIC_QUEUE_COMMAND_WAIT_FADE_IN:
        if (--g_music_queue.current_value != 0) {
            return;
        }
        g_music_queue.current_command = MUSIC_QUEUE_COMMAND_NONE;
        g_music_queue.music.flags |= MUSIC_QUEUE_PLAYING;
        break;
    }

    goto next;
}

/* Queue a music change: fade out what is playing, start `value` (a packed
 * slot and music id) and fade it in over 0x10 steps. */
void music_queue_track_start(s32 value) {
    if (g_music_queue.music.flags & MUSIC_QUEUE_PLAYING) {
        music_queue_add(MUSIC_QUEUE_COMMAND_FADE_OUT, 0x10);
    }
    music_queue_add(MUSIC_QUEUE_COMMAND_PLAY_TRACK, value);
    music_queue_add(MUSIC_QUEUE_COMMAND_FADE_IN, 0x10);
}

/* Twin of file_pending_wait with the music queue as the wait condition:
 * run frames until no command is queued or running. The two 32-bit busy
 * fields are read as g_music_queue members rather than as separate
 * scalars. */
void music_queue_wait_until_empty(void) {
    while (g_music_queue.count != 0 || g_music_queue.current_command != MUSIC_QUEUE_COMMAND_NONE) {
        file_load_poll(&g_file_cd_state);
        music_queue_process();
        VSync(0);
    }
}
