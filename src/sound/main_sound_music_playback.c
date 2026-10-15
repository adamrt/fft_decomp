/* Music slot loading, switching, volume and tunes. */

#include "fft/main.h"
#include "psx/types.h"

/* Loads music file music_id (main_music_e) into a free music slot, waiting
 * for the read, and creates its music record without starting it
 * (main_sound_switch_music_track starts a slot). Returns the slot, or 0 when
 * it was occupied. */
s32 main_sound_open_and_play_music(s32 music_id, s32 slot) {
    suzuki_smd_header_t* smd;

    if (g_main_sound_music.slots.smd[slot - 1] == 0) {
        smd = main_file_get_smd(
            g_main_sound_scenario_smd_files[music_id].sector, g_main_sound_scenario_smd_files[music_id].size);
        g_main_sound_music.slots.smd[slot - 1] = smd;
        g_main_sound_music.slots.music[slot - 1] = suzuki_smd_create_music(smd);
        return slot;
    }
    return 0;
}

/* main_sound_open_and_play_music into the first free scenario slot (1 or
 * 2); returns 0 when both are taken. */
int main_sound_open_music_into_free_slot(int music_id) {
    int i;
    suzuki_smd_header_t* smd;

    for (i = 0; i < 2; i++) {
        if (g_main_sound_music.slots.smd[i] == 0) {
            smd = main_file_get_smd(
                g_main_sound_scenario_smd_files[music_id].sector, g_main_sound_scenario_smd_files[music_id].size);
            g_main_sound_music.slots.smd[i] = smd;
            g_main_sound_music.slots.music[i] = suzuki_smd_create_music(smd);
            return i + 1;
        }
    }
    return 0;
}

/* Always 0. Callers poll it after opening music and start the track once it returns 0. */
int main_sound_is_music_loading_stub(void) {
    return 0;
}

/* Records the slot that was just opened (loading_slot, 0x8004d970). The
 * do/while(0) wrapper is load-bearing: the retail allocation (slot in s2,
 * the slot pointer in s3) needs this use of `slot` weighted as loop-nested,
 * which is what a statement macro of this shape produces. */
#define set_opened_music_slot(slot)                                                                                    \
    do {                                                                                                               \
        g_main_sound_music.slots.loading_slot = slot;                                                                  \
    } while (0)

/* Starts the asynchronous load of music file `music_id` (main_music_e) into
 * music slot `slot` (1-based) if the slot is free. Returns the slot, or 0 when it was occupied or the load
 * failed (which also raises exception 0x11). */
s32 main_sound_open_music_into_slot(s32 music_id, s32 slot) {
    if (g_main_sound_music.slots.smd[slot - 1] == 0) {
        suzuki_smd_header_t* data = main_heap_alloc_smd(g_main_sound_scenario_smd_files[music_id].size);

        if (data != 0
            && main_file_call_build_header(g_main_sound_scenario_smd_files[music_id].sector,
                   g_main_sound_scenario_smd_files[music_id].size, data)
                == 0) {
            g_main_sound_music.slots.smd[slot - 1] = data;
            set_opened_music_slot(slot);
            return slot;
        }
        main_system_handle_error(MAIN_SYSTEM_ERROR_SMD_LOAD);
        return 0;
    }
    return 0;
}

/* Starts the asynchronous load of music file `music_id` (main_music_e) into
 * the first free scenario music slot (1 or 2) and returns that slot, or 0
 * when both are taken or the load fails. main_sound_poll_scenario_smd_load
 * creates the music record once the file has arrived. */
int main_sound_open_scenario_smd_files(int music_id) {
    int i;
    suzuki_smd_header_t* buffer;

    for (i = 0; i < 2; i++) {
        if (g_main_sound_music.slots.smd[i] == 0) {
            buffer = main_heap_alloc_smd(g_main_sound_scenario_smd_files[music_id].size);
            if (buffer != 0) {
                if (main_file_call_build_header(g_main_sound_scenario_smd_files[music_id].sector,
                        g_main_sound_scenario_smd_files[music_id].size, buffer)
                    == 0) {
                    g_main_sound_music.slots.smd[i] = buffer;
                    g_main_sound_music.slots.loading_slot = i + 1;
                } else {
                    main_system_handle_error(MAIN_SYSTEM_ERROR_SMD_LOAD);
                    return 0;
                }
            } else {
                main_system_handle_error(MAIN_SYSTEM_ERROR_SMD_LOAD);
                return 0;
            }
            return i + 1;
        }
    }
    return 0;
}

/* Finish the asynchronous scenario load and create the music record for
 * loading_slot (1 or 2). */
int main_sound_poll_scenario_smd_load(void) {
    if (g_main_sound_music.slots.loading_slot == 0) {
        return 0;
    }
    if (main_file_is_still_loading() != 0) {
        return 1;
    }
    g_main_sound_music.slots.music[g_main_sound_music.slots.loading_slot - 1]
        = suzuki_smd_create_music(g_main_sound_music.slots.smd[g_main_sound_music.slots.loading_slot - 1]);
    g_main_sound_music.slots.loading_slot = 0;
    return 0;
}

/* Blocking form of main_sound_open_scenario_smd_files: polls the loader each
 * frame until the music record exists. Returns the slot, or 0 when no load
 * started. */
s32 main_sound_wait_for_scenario_smd_load(s32 music_id) {
    s32 result;

    result = main_sound_open_scenario_smd_files(music_id);
    if (result != 0) {
        while (main_sound_poll_scenario_smd_load() != 0) {
            VSync(0);
            main_system_frame_stub();
            main_file_poll_load(&g_main_file_cd_state);
        }
    }
    return result;
}

/* Frees the music record and SMD data of a slot. It does not clear the
 * current-music state when the slot is the playing one. */
s32 main_sound_unload_scenario_mus(s32 slot) {
    if (g_main_sound_music.slots.smd[slot - 1] != 0) {
        suzuki_smd_unload_music(g_main_sound_music.slots.music[slot - 1]);
        g_main_sound_music.slots.music[slot - 1] = 0;
        main_heap_free_smd(g_main_sound_music.slots.smd[slot - 1]);
        g_main_sound_music.slots.smd[slot - 1] = 0;
        return 1;
    }
    return 0;
}

/* Keys off the playing scenario music and clears the current-music state;
 * its slot stays loaded. */
s32 main_sound_stop_current_music(void) {
    if (g_main_sound_music.state.current_music != 0) {
        suzuki_smd_stop_music(g_main_sound_music.state.current_music);
        g_main_sound_music.state.current_slot = 0;
        g_main_sound_music.state.current_smd = 0;
        g_main_sound_music.state.current_music = 0;
        return 1;
    }
    return 0;
}

/* Stops the current music and restarts the music loaded in slot from its
 * beginning, fading in to volume over fade ticks. Returns the music record
 * as a handle, or 0 for slot 0. */
suzuki_music_t* main_sound_switch_music_track(s32 slot, s32 volume, s32 fade) {
    main_sound_stop_current_music();
    if (slot != 0) {
        if (g_main_sound_music.slots.smd[slot - 1] != 0) {
            g_main_sound_music.state.current_slot = slot;
            g_main_sound_music.state.current_smd = g_main_sound_music.slots.smd[slot - 1];
            g_main_sound_music.state.current_music = g_main_sound_music.slots.music[slot - 1];
            g_main_sound_music.state.volume = volume;
            suzuki_smd_reset_music(g_main_sound_music.state.current_music, volume, fade);
        }
        return g_main_sound_music.state.current_music;
    }
    return 0;
}

/* Returns the slot of the current music (g_main_sound_music.state.current_slot),
 * not a music id. */
int main_sound_get_current_scenario_music(void) {
    return g_main_sound_music.state.current_slot;
}

/* Fades the current music to volume over time ticks without remembering
 * the volume. */
int main_sound_set_current_music_volume(int volume, int time) {
    if (g_main_sound_music.state.current_music != 0) {
        suzuki_smd_set_music_volume(g_main_sound_music.state.current_music, volume, time);
        return 1;
    }
    return 0;
}

/* Fades the current music back to the remembered volume over time ticks. */
int main_sound_set_current_music_volume_timed(int time) {
    if (g_main_sound_music.state.current_music != 0) {
        suzuki_smd_set_music_volume(g_main_sound_music.state.current_music, g_main_sound_music.state.volume, time);
        return 1;
    }
    return 0;
}

/* Fades the current music to volume over time ticks and remembers the
 * volume for main_sound_set_current_music_volume_timed. */
int main_sound_set_current_music_target(int volume, int time) {
    if (g_main_sound_music.state.current_music != 0) {
        g_main_sound_music.state.volume = volume;
        suzuki_smd_set_music_volume(g_main_sound_music.state.current_music, volume, time);
        return 1;
    }
    return 0;
}

int main_sound_set_current_music_tempo_scale_timed(int value, int time) {
    if (g_main_sound_music.state.current_music != 0) {
        suzuki_smd_set_tempo_scale(g_main_sound_music.state.current_music, value, time);
        return 1;
    }
    return 0;
}

int main_sound_set_current_music_pitch_shift_timed(int value, int time) {
    if (g_main_sound_music.state.current_music != 0) {
        suzuki_smd_set_pitch_shift(g_main_sound_music.state.current_music, value, time);
        return 1;
    }
    return 0;
}

int main_sound_set_current_music_balance_shift_timed(int value, int time) {
    if (g_main_sound_music.state.current_music != 0) {
        suzuki_smd_set_balance_shift(g_main_sound_music.state.current_music, value, time);
        return 1;
    }
    return 0;
}

/* Requests a tune (main_sound_tune_e) unless one is pending or playing: the
 * scenario music fades out and main_sound_update_tunes starts the tune once
 * it has stopped. */
int main_sound_play_tune(int tune) {
    if (tune != 0) {
        if (g_main_sound_music.slots.tune == 0) {
            main_sound_set_current_music_target(0, 120);
            g_main_sound_music.slots.tune = tune | MAIN_SOUND_TUNE_PENDING;
        }
    }
    return 1;
}

/* Cancels a pending tune, or fades a playing one out to volume 0 over time
 * ticks (0: at once). */
int main_sound_set_tune_volume(int time) {
    int tune = g_main_sound_music.slots.tune;

    if (tune != 0) {
        if ((tune & MAIN_SOUND_TUNE_PENDING) != 0) {
            g_main_sound_music.slots.tune = 0;
        } else {
            suzuki_smd_set_music_volume(g_main_sound_music.slots.music[1 + (tune & MAIN_SOUND_TUNE_ID_MASK)], 0, time);
        }
    }
    return 1;
}

/* Advances the play-tune state machine once per frame: a pending tune
 * (MAIN_SOUND_TUNE_PENDING) starts once the scenario music has stopped; a playing tune is
 * torn down once it ends and the scenario music is faded back in. The tune
 * at `tune_index` lives in music slot 2 + `tune_index`. */
s32 main_sound_update_tunes(void) {
    s32 tune;
    s32 tune_index;

    tune = g_main_sound_music.slots.tune;
    if (tune & MAIN_SOUND_TUNE_PENDING) {
        suzuki_music_t** tune_handles;
        suzuki_music_t** handle;

        if (suzuki_smd_is_music_playing(g_main_sound_music.state.current_music) != 0) {
            return 0;
        }
        tune_index = g_main_sound_music.slots.tune & MAIN_SOUND_TUNE_ID_MASK;
        g_main_sound_music.slots.tune &= 0x7f;
        tune_handles = &g_main_sound_music.slots.music[1];
        handle = &tune_handles[tune_index];
        if (*handle == 0) {
            *handle = suzuki_smd_create_music(g_main_sound_music.slots.smd[1 + tune_index]);
        }
        suzuki_smd_reset_music(*handle, 0x7f, 0);
        return 1;
    }
    if (tune != 0) {
        suzuki_music_t** tune_handles;
        suzuki_music_t** handle;

        tune_index = tune & MAIN_SOUND_TUNE_ID_MASK;
        tune_handles = &g_main_sound_music.slots.music[1];
        handle = &tune_handles[tune_index];
        if (suzuki_smd_is_music_playing(*handle) != 0) {
            return 0;
        }
        suzuki_smd_stop_music(*handle);
        if (*handle != 0) {
            suzuki_smd_unload_music(*handle);
        }
        *handle = 0;
        g_main_sound_music.slots.tune = 0;
        return main_sound_set_current_music_target(0x7f, 0x78);
    }
    return 0;
}

void main_sound_unload_scenario_music_and_tunes(void) {
    main_sound_stop_current_music();
    main_sound_unload_scenario_mus(1);
    main_sound_unload_scenario_mus(2);
    main_sound_set_tune_volume(0);
}

suzuki_music_t* main_sound_get_music_handle(int slot) {
    return g_main_sound_music.slots.music[slot - 1];
}

void main_sound_unload_current_scenario_music(void) {
    int slot = g_main_sound_music.state.current_slot;

    main_sound_stop_current_music();
    main_sound_unload_scenario_mus(slot);
}

/* Replaces the playing scenario music: stops and unloads it, loads music
 * file music_id (main_music_e) into a free slot and starts it at full
 * volume. */
void main_sound_set_current_music_track(int music_id) {
    int slot;

    if (g_main_sound_music.state.current_smd != 0) {
        slot = g_main_sound_music.state.current_slot;
        main_sound_stop_current_music();
        main_sound_unload_scenario_mus(slot);
    }
    slot = main_sound_open_music_into_free_slot(music_id);
    if (slot != 0) {
        main_sound_switch_music_track(slot, 0x7f, 0);
    }
}
