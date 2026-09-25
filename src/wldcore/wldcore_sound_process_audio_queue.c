#include "fft/wldcore.h"

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
    case 0:
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
    case 1:
        if (g_main_file_still_loading != 0) {
            return;
        }
        slot = (g_wldcore_audio_queue.current_value & 0xF00) >> 8;
        track = g_wldcore_audio_queue.current_value & 0xFF;
        slot_track = &g_wldcore_audio_queue.music.tracks[slot];
        if (*slot_track == track) {
            g_wldcore_audio_queue.current_slot = slot;
            g_wldcore_audio_queue.current_command = 0;
            main_sound_switch_music_track(slot, 0, 0);
            return;
        }
        if (*slot_track != 0) {
            main_sound_unload_scenario_mus(slot);
        }
        if (main_sound_open_and_play_music(track, slot) != 0) {
            g_wldcore_audio_queue.current_command = 0x10;
            g_wldcore_audio_queue.current_slot = slot;
            *slot_track = track;
            return;
        }
        g_wldcore_audio_queue.current_command = 0;
        return;
    case 2:
        if ((g_wldcore_audio_queue.music.flags & 1) == 0) {
            goto clear_command;
        }
        main_sound_set_current_music_target(0, g_wldcore_audio_queue.current_value << 2);
        g_wldcore_audio_queue.current_command = 0x11;
        return;
    case 3:
        main_sound_switch_music_track(g_wldcore_audio_queue.current_slot, 0, 0);
        main_sound_set_current_music_target(g_wldcore_audio_queue.volume, g_wldcore_audio_queue.current_value << 2);
        g_wldcore_audio_queue.current_command = 0x12;
        return;
    case 4:
        value = g_wldcore_audio_queue.current_value;
        if (value != 0) {
            slot = value;
        } else {
            slot = g_wldcore_audio_queue.current_slot;
        }
        main_sound_unload_scenario_mus(slot);
        if (slot == g_wldcore_audio_queue.current_slot) {
            g_wldcore_audio_queue.music.flags &= ~1;
        }
        g_wldcore_audio_queue.music.tracks[slot] = 0;
    default:
    clear_command:
        g_wldcore_audio_queue.current_command = 0;
        break;
    case 0x10:
        if (g_main_file_still_loading == 0) {
            goto clear_command;
        }
        return;
    case 0x11:
        if (--g_wldcore_audio_queue.current_value != 0) {
            return;
        }
        g_wldcore_audio_queue.music.flags &= ~1;
        main_sound_stop_forced_music();
        goto clear_command;
    case 0x12:
        if (--g_wldcore_audio_queue.current_value != 0) {
            return;
        }
        g_wldcore_audio_queue.current_command = 0;
        g_wldcore_audio_queue.music.flags |= 1;
        break;
    }
    goto next;
}
