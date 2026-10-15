#include "fft/wldcore.h"
#include "psx/types.h"

/* Apply and clear the pending signed delta, then publish a bounded value.
 *
 * The consumer requires a 16-bit unsigned range. State flag 0x04 marks the
 * update complete; the surrounding dispatch table has not identified this
 * handler's script opcode yet. */
void wldcore_opcode_apply_pending_delta_and_clamp_value(void) {
    s32 delta;
    s32* value = &g_wldcore_active_saved_record.counter;

    delta = g_wldcore_active_saved_record.counter_delta;
    g_wldcore_active_saved_record.counter_delta = 0;
    *value += delta;
    if (*value > 0xFFFF) {
        *value = 0xFFFF;
    }
    if (*value < 0) {
        *value = 0;
    }
    g_wldcore_displayed_numeric_value = *value;
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

void wldcore_opcode_play_sound(void) {
    main_sound_play_sfx_find_channel((u8)(g_wldcore_active_saved_record.instruction >> 8));
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

void wldcore_opcode_play_music(void) {
    wldcore_sound_change_scenario_music((u8)(g_wldcore_active_saved_record.instruction >> 8));
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Replace the sound novel's music with `music_id`: stop the playing track,
 * unload its slot (slot 2 is used when none is loaded) and start the new
 * track; the load-wait loop after it polls a stub that always returns 0. The saved record keeps the slot in the high
 * byte and the music id in the low byte. */
void wldcore_sound_change_scenario_music(s32 music_id) {
    s16 packed = g_wldcore_active_saved_record.music_selection;
    s32 slot = packed >> 8;

    if ((packed & 0xFF) != 0) {
        main_sound_stop_current_music();
    }
    if (slot != 0) {
        main_sound_unload_scenario_mus(slot);
    } else {
        slot = 2;
    }
    VSync(2);
    main_sound_open_and_play_music(music_id, slot);
    g_wldcore_active_saved_record.music_selection = (slot << 8) | music_id;
    while (main_sound_is_music_loading_stub() != 0) {
        VSync(0);
    }
    g_wldcore_audio_queue.music.tracks[slot - 1] = music_id;
    g_wldcore_audio_queue.current_slot = slot;
}
