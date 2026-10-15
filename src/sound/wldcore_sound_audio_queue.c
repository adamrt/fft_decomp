/* Audio command FIFO. */

#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_sound_play_effect(s32 sound_id) {
    main_sound_play_sfx(sound_id);
}

/* Queue an audio command, servicing frame work while the queue is full.
 *
 * Accessing the count as a g_wldcore_audio_queue member is what makes GCC reload it
 * after each array store. */
void wldcore_sound_enqueue_audio_command(wldcore_audio_command_e command, s32 value) {
    while (g_wldcore_audio_queue.count >= WLDCORE_AUDIO_QUEUE_CAPACITY) {
        main_file_poll_load(&g_main_file_cd_state);
        wldcore_sound_process_audio_queue();
        VSync(0);
    }

    g_wldcore_audio_queue.commands[g_wldcore_audio_queue.count] = command;
    g_wldcore_audio_queue.values[g_wldcore_audio_queue.count] = value;
    g_wldcore_audio_queue.count++;
}

/* Services the audio command FIFO: dequeues the next command when idle and
 * runs the current one until it has to wait for loading or a fade.
 *
 * Commands: 1 loads and plays track `value & 0xff` into slot `value >> 8`,
 * 2 fades the current music out when the flag is set, 3 switches to the
 * current slot and fades in to `volume`, 4 unloads a slot (0 = current);
 * 0x10-0x12 wait for the load or the fade timer. The dispatch loop is a
 * backward goto: a `for (;;)` loop lets loop.c hoist the queue addresses
 * into saved registers, which the target never does. The if/else in case 4
 * keeps CSE from carrying the value's address across the join. Cases 2,
 * 0x10 and 0x11 jump into the default arm's reset as the target does; a
 * duplicated reset is cross-jumped into the last copy instead. */
void wldcore_sound_process_audio_queue(void) {
    s32 i;
    s32 value;
    s32 slot;
    s32 track;
    s32* slot_track;

next:
    switch (g_wldcore_audio_queue.current_command) {
    case WLDCORE_AUDIO_COMMAND_NONE:
        if (g_wldcore_audio_queue.count == 0) {
            return;
        }
        g_wldcore_audio_queue.current_command = g_wldcore_audio_queue.commands[0];
        g_wldcore_audio_queue.current_value = g_wldcore_audio_queue.values[0];
        for (i = 0; i < g_wldcore_audio_queue.count - 1; i++) {
            g_wldcore_audio_queue.commands[i] = g_wldcore_audio_queue.commands[i + 1];
            g_wldcore_audio_queue.values[i] = g_wldcore_audio_queue.values[i + 1];
        }
        g_wldcore_audio_queue.count--;
        break;
    case WLDCORE_AUDIO_COMMAND_PLAY_MUSIC:
        if (g_main_file_still_loading != 0) {
            return;
        }
        slot = (g_wldcore_audio_queue.current_value & 0xF00) >> 8;
        track = g_wldcore_audio_queue.current_value & 0xFF;
        slot_track = &g_wldcore_audio_queue.music.tracks[slot - 1];
        if (*slot_track == track) {
            g_wldcore_audio_queue.current_slot = slot;
            g_wldcore_audio_queue.current_command = WLDCORE_AUDIO_COMMAND_NONE;
            main_sound_switch_music_track(slot, 0, 0);
            return;
        }
        if (*slot_track != 0) {
            main_sound_unload_scenario_mus(slot);
        }
        if (main_sound_open_and_play_music(track, slot) != 0) {
            g_wldcore_audio_queue.current_command = WLDCORE_AUDIO_COMMAND_WAIT_LOAD;
            g_wldcore_audio_queue.current_slot = slot;
            *slot_track = track;
            return;
        }
        g_wldcore_audio_queue.current_command = WLDCORE_AUDIO_COMMAND_NONE;
        return;
    case WLDCORE_AUDIO_COMMAND_FADE_OUT:
        if ((g_wldcore_audio_queue.music.flags & WLDCORE_AUDIO_MUSIC_PLAYING) == 0) {
            goto clear_command;
        }
        main_sound_set_current_music_target(0, g_wldcore_audio_queue.current_value << 2);
        g_wldcore_audio_queue.current_command = WLDCORE_AUDIO_COMMAND_WAIT_FADE_OUT;
        return;
    case WLDCORE_AUDIO_COMMAND_FADE_IN:
        main_sound_switch_music_track(g_wldcore_audio_queue.current_slot, 0, 0);
        main_sound_set_current_music_target(g_wldcore_audio_queue.volume, g_wldcore_audio_queue.current_value << 2);
        g_wldcore_audio_queue.current_command = WLDCORE_AUDIO_COMMAND_WAIT_FADE_IN;
        return;
    case WLDCORE_AUDIO_COMMAND_UNLOAD:
        value = g_wldcore_audio_queue.current_value;
        if (value != 0) {
            slot = value;
        } else {
            slot = g_wldcore_audio_queue.current_slot;
        }
        main_sound_unload_scenario_mus(slot);
        if (slot == g_wldcore_audio_queue.current_slot) {
            g_wldcore_audio_queue.music.flags &= ~WLDCORE_AUDIO_MUSIC_PLAYING;
        }
        g_wldcore_audio_queue.music.tracks[slot - 1] = 0;
    default:
    clear_command:
        g_wldcore_audio_queue.current_command = WLDCORE_AUDIO_COMMAND_NONE;
        break;
    case WLDCORE_AUDIO_COMMAND_WAIT_LOAD:
        if (g_main_file_still_loading == 0) {
            goto clear_command;
        }
        return;
    case WLDCORE_AUDIO_COMMAND_WAIT_FADE_OUT:
        if (--g_wldcore_audio_queue.current_value != 0) {
            return;
        }
        g_wldcore_audio_queue.music.flags &= ~WLDCORE_AUDIO_MUSIC_PLAYING;
        main_sound_stop_current_music();
        goto clear_command;
    case WLDCORE_AUDIO_COMMAND_WAIT_FADE_IN:
        if (--g_wldcore_audio_queue.current_value != 0) {
            return;
        }
        g_wldcore_audio_queue.current_command = WLDCORE_AUDIO_COMMAND_NONE;
        g_wldcore_audio_queue.music.flags |= WLDCORE_AUDIO_MUSIC_PLAYING;
        break;
    }
    goto next;
}

/* Queue a music change: fade out what is playing, start `value` (a packed
 * slot and music id) and fade it in over 0x10 steps. */
void wldcore_sound_enqueue_music_start(s32 value) {
    if (g_wldcore_audio_queue.music.flags & WLDCORE_AUDIO_MUSIC_PLAYING) {
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x10);
    }
    wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_PLAY_MUSIC, value);
    wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_IN, 0x10);
}

/* Twin of wldcore_wait_for_file_load (0x800686c8) with the audio-queue depth
 * as the wait condition. The two 32-bit busy fields are read as g_wldcore_audio_queue members
 * rather than as separate scalars. */
void wldcore_sound_wait_for_queue_drain(void) {
    while (g_wldcore_audio_queue.count != 0 || g_wldcore_audio_queue.current_command != WLDCORE_AUDIO_COMMAND_NONE) {
        main_file_poll_load(&g_main_file_cd_state);
        wldcore_sound_process_audio_queue();
        VSync(0);
    }
}
