/* Scenario music loading, playback and parameters, resident tunes and sound effects. */

#include "fft/file.h"
#include "fft/memory.h"
#include "fft/music.h"
#include "fft/sound.h"
#include "fft/suzuki.h"
#include "fft/system.h"
#include "psx/libetc.h"
#include "psx/types.h"

extern u8 g_sound_system_sed_data[];
extern u8 g_sound_env_sed_data[];
static s32 g_sound_background_sfx_id; /* the background sound sound_background_play started, 0 for none */

/* Scenario-music playback state. current_music is the Suzuki music record of
 * the playing scenario music: music_stop hands it straight
 * to suzuki_smd_stop_music, which takes a suzuki_music_t*. */
typedef struct music_state {
    s32 current_slot;                 /* 0x00: slot of the playing scenario music */
    suzuki_music_t* current_music;    /* 0x04 */
    suzuki_smd_header_t* current_smd; /* 0x08: SMD data of the playing music */
    s32 target_volume;                /* 0x0c: volume restored after a temporary fade */
} music_state_t;
typedef char music_state_size_must_be_16[sizeof(music_state_t) == 16 ? 1 : -1];

/* Music slots 1..2 hold scenario music; slots 3..7 hold the five permanent
 * tunes. Both arrays store slot n at index n - 1. */
typedef struct music_slots {
    s32 tune;                    /* 0x00: pending tune (id | 0x80) or playing tune id */
    s32 loading_slot;            /* 0x04: slot whose SMD load is in flight, or zero */
    suzuki_music_t* music[7];    /* 0x08: music records returned by suzuki_smd_create_music */
    suzuki_smd_header_t* smd[7]; /* 0x24: loaded SMD data */
} music_slots_t;
typedef char music_slots_size_must_be_64[sizeof(music_slots_t) == 64 ? 1 : -1];

/* The music state and slot table are one object: sound_init
 * derives the slot pointers from the &state.current_smd base register, and the
 * tune/slot helpers reach across tune, handles and smd from single bases. */
typedef struct music {
    music_state_t state; /* 0x00 */
    music_slots_t slots; /* 0x10 */
} music_t;
typedef char music_size_must_be_80[sizeof(music_t) == 80 ? 1 : -1];

extern music_t g_music;

/* MUSIC_00.SMD through MUSIC_99.SMD; entries 45..49 are the resident tunes. */
static file_extent_t g_music_files[MUSIC_COUNT] = {
    { 85249, 10240 },
    { 85254, 10240 },
    { 85259, 6144 },
    { 85262, 6144 },
    { 85265, 6144 },
    { 85268, 6144 },
    { 85271, 6144 },
    { 85274, 10240 },
    { 85279, 6144 },
    { 85282, 4096 },
    { 85284, 6144 },
    { 85287, 10240 },
    { 85292, 10240 },
    { 85297, 8192 },
    { 85301, 14336 },
    { 85308, 8192 },
    { 85312, 4096 },
    { 85314, 8192 },
    { 85318, 18432 },
    { 85327, 16384 },
    { 85335, 2048 },
    { 85336, 2048 },
    { 85337, 2048 },
    { 85338, 2048 },
    { 85339, 2048 },
    { 85340, 4096 },
    { 85342, 6144 },
    { 85345, 2048 },
    { 85346, 4096 },
    { 85348, 4096 },
    { 85350, 2048 },
    { 85351, 4096 },
    { 85353, 2048 },
    { 85354, 2048 },
    { 85355, 4096 },
    { 85357, 2048 },
    { 85358, 4096 },
    { 85360, 10240 },
    { 85365, 8192 },
    { 85369, 6144 },
    { 85372, 2048 },
    { 85373, 2048 },
    { 85374, 4096 },
    { 85376, 2048 },
    { 85377, 2048 },
    { 85378, 2048 },
    { 85379, 2048 },
    { 85380, 2048 },
    { 85381, 2048 },
    { 85382, 2048 },
    { 85383, 2048 },
    { 85384, 2048 },
    { 85385, 4096 },
    { 85387, 4096 },
    { 85389, 4096 },
    { 85391, 2048 },
    { 85392, 4096 },
    { 85394, 4096 },
    { 85396, 4096 },
    { 85398, 2048 },
    { 85399, 4096 },
    { 85401, 4096 },
    { 85403, 6144 },
    { 85406, 2048 },
    { 85407, 4096 },
    { 85409, 8192 },
    { 85413, 6144 },
    { 85416, 4096 },
    { 85418, 4096 },
    { 85420, 4096 },
    { 85422, 4096 },
    { 85424, 6144 },
    { 85427, 6144 },
    { 85430, 2048 },
    { 85431, 4096 },
    { 85433, 4096 },
    { 85435, 10240 },
    { 85440, 10240 },
    { 85445, 6144 },
    { 85448, 2048 },
    { 85449, 6144 },
    { 85452, 14336 },
    { 85459, 10240 },
    { 85464, 6144 },
    { 85467, 8192 },
    { 85471, 4096 },
    { 85473, 6144 },
    { 85476, 4096 },
    { 85478, 4096 },
    { 85480, 4096 },
    { 85482, 6144 },
    { 85485, 12288 },
    { 85491, 2048 },
    { 85492, 6144 },
    { 85495, 4096 },
    { 85497, 10240 },
    { 85502, 4096 },
    { 85504, 4096 },
    { 85506, 4096 },
    { 85508, 20480 },
};

/* Load scenario music `music_id` into scenario slot `slot` (1 or 2) when that slot is
 * free and return the slot; return 0 when it is taken. */
s32 music_slot_load(s32 music_id, s32 slot) {
    suzuki_smd_header_t* smd;

    if (g_music.slots.smd[slot - 1] == 0) {
        smd = file_smd_load(g_music_files[music_id].sector, g_music_files[music_id].size);
        g_music.slots.smd[slot - 1] = smd;
        g_music.slots.music[slot - 1] = suzuki_smd_create_music(smd);
        return slot;
    }
    return 0;
}

/* music_slot_load into the first free scenario slot (1 or
 * 2); returns 0 when both are taken. */
int music_free_slot_load(int music_id) {
    int i;
    suzuki_smd_header_t* smd;

    for (i = 0; i < 2; i++) {
        if (g_music.slots.smd[i] == 0) {
            smd = file_smd_load(g_music_files[music_id].sector, g_music_files[music_id].size);
            g_music.slots.smd[i] = smd;
            g_music.slots.music[i] = suzuki_smd_create_music(smd);
            return i + 1;
        }
    }
    return 0;
}

/* Always 0. Callers poll it after opening music and start the track once it returns 0. */
int music_is_loading_stub(void) {
    return 0;
}

/* Records the slot that was just opened (loading_slot, 0x8004d970). The
 * do/while(0) wrapper is load-bearing: the retail allocation (slot in s2,
 * the slot pointer in s3) needs this use of `slot` weighted as loop-nested,
 * which is what a statement macro of this shape produces. */
#define set_opened_music_slot(slot)                                                                                    \
    do {                                                                                                               \
        g_music.slots.loading_slot = slot;                                                                             \
    } while (0)

/* Starts the asynchronous load of music file `music_id` (music_id_e) into
 * music slot `slot` (1-based) if the slot is free. Returns the slot, or 0 when
 * it was occupied or the load failed, which also reports
 * SYSTEM_ERROR_SMD_LOAD. When the read cannot start (the loader is busy)
 * the SMD block stays allocated and is never freed. */
s32 music_slot_load_start(s32 music_id, s32 slot) {
    if (g_music.slots.smd[slot - 1] == 0) {
        suzuki_smd_header_t* data = heap_smd_alloc(g_music_files[music_id].size);

        if (data != 0
            && file_bytes_read_request(g_music_files[music_id].sector, g_music_files[music_id].size, data) == 0) {
            g_music.slots.smd[slot - 1] = data;
            set_opened_music_slot(slot);
            return slot;
        }
        system_error_handle(SYSTEM_ERROR_SMD_LOAD);
        return 0;
    }
    return 0;
}

/* Starts the asynchronous load of music file `music_id` (music_id_e) into
 * the first free scenario music slot (1 or 2) and returns that slot, or 0
 * when both are taken or the load fails (reporting SYSTEM_ERROR_SMD_LOAD;
 * as in music_slot_load_start, a read that cannot start leaks its
 * SMD block). music_scenario_load_poll creates the music record once
 * the file has arrived. */
int music_free_slot_load_start(int music_id) {
    int i;
    suzuki_smd_header_t* buffer;

    for (i = 0; i < 2; i++) {
        if (g_music.slots.smd[i] == 0) {
            buffer = heap_smd_alloc(g_music_files[music_id].size);
            if (buffer != 0) {
                if (file_bytes_read_request(g_music_files[music_id].sector, g_music_files[music_id].size, buffer)
                    == 0) {
                    g_music.slots.smd[i] = buffer;
                    g_music.slots.loading_slot = i + 1;
                } else {
                    system_error_handle(SYSTEM_ERROR_SMD_LOAD);
                    return 0;
                }
            } else {
                system_error_handle(SYSTEM_ERROR_SMD_LOAD);
                return 0;
            }
            return i + 1;
        }
    }
    return 0;
}

/* Finish the asynchronous scenario load and create the music record for
 * loading_slot (1 or 2). */
int music_scenario_load_poll(void) {
    if (g_music.slots.loading_slot == 0) {
        return 0;
    }
    if (file_load_state_get() != 0) {
        return 1;
    }
    g_music.slots.music[g_music.slots.loading_slot - 1]
        = suzuki_smd_create_music(g_music.slots.smd[g_music.slots.loading_slot - 1]);
    g_music.slots.loading_slot = 0;
    return 0;
}

/* Blocking form of music_free_slot_load_start: polls the loader each
 * frame until the music record exists. Returns the slot, or 0 when no load
 * started. */
s32 music_scenario_load_wait(s32 music_id) {
    s32 result = music_free_slot_load_start(music_id);

    if (result != 0) {
        while (music_scenario_load_poll() != 0) {
            VSync(0);
            system_frame_stub();
            file_load_poll(&g_file_cd_state);
        }
    }
    return result;
}

/* Frees the music record and SMD data of a slot. It does not clear the
 * current-music state when the slot is the playing one. */
s32 music_scenario_slot_unload(s32 slot) {
    if (g_music.slots.smd[slot - 1] != 0) {
        suzuki_smd_unload_music(g_music.slots.music[slot - 1]);
        g_music.slots.music[slot - 1] = 0;
        heap_smd_free(g_music.slots.smd[slot - 1]);
        g_music.slots.smd[slot - 1] = 0;
        return 1;
    }
    return 0;
}

/* Keys off the playing scenario music and clears the current-music state;
 * its slot stays loaded. */
s32 music_stop(void) {
    if (g_music.state.current_music != 0) {
        suzuki_smd_stop_music(g_music.state.current_music);
        g_music.state.current_slot = 0;
        g_music.state.current_smd = 0;
        g_music.state.current_music = 0;
        return 1;
    }
    return 0;
}

/* Stops the current music and restarts the music loaded in slot from its
 * beginning, fading in to volume over fade ticks. Returns the music record
 * as a handle, or 0 for slot 0. */
suzuki_music_t* music_track_switch(s32 slot, s32 volume, s32 fade) {
    music_stop();
    if (slot != 0) {
        if (g_music.slots.smd[slot - 1] != 0) {
            g_music.state.current_slot = slot;
            g_music.state.current_smd = g_music.slots.smd[slot - 1];
            g_music.state.current_music = g_music.slots.music[slot - 1];
            g_music.state.target_volume = volume;
            suzuki_smd_reset_music(g_music.state.current_music, volume, fade);
        }
        return g_music.state.current_music;
    }
    return 0;
}

/* Returns the slot of the current music (g_music.state.current_slot),
 * not a music id. */
int music_current_slot_get(void) {
    return g_music.state.current_slot;
}

/* Fades the current music to volume over time ticks without remembering
 * the volume. */
int music_volume_set(int volume, int time) {
    if (g_music.state.current_music != 0) {
        suzuki_smd_set_music_volume(g_music.state.current_music, volume, time);
        return 1;
    }
    return 0;
}

/* Fades the current music back to the remembered volume over time ticks. */
int music_volume_restore(int time) {
    if (g_music.state.current_music != 0) {
        suzuki_smd_set_music_volume(g_music.state.current_music, g_music.state.target_volume, time);
        return 1;
    }
    return 0;
}

/* Fades the current music to volume over time ticks and remembers the
 * volume for music_volume_restore. */
int music_volume_target_set(int volume, int time) {
    if (g_music.state.current_music != 0) {
        g_music.state.target_volume = volume;
        suzuki_smd_set_music_volume(g_music.state.current_music, volume, time);
        return 1;
    }
    return 0;
}

/* Change the current music's tempo scale to value over time ticks; returns 0 when no music is
 * current. Nothing in the tree calls it. */
int music_tempo_scale_fade(int value, int time) {
    if (g_music.state.current_music != 0) {
        suzuki_smd_set_tempo_scale(g_music.state.current_music, value, time);
        return 1;
    }
    return 0;
}

/* Shift the current music's pitch to value over time ticks; returns 0 when no music is current.
 * Nothing in the tree calls it. */
int music_pitch_shift_fade(int value, int time) {
    if (g_music.state.current_music != 0) {
        suzuki_smd_set_pitch_shift(g_music.state.current_music, value, time);
        return 1;
    }
    return 0;
}

/* Shift the current music's balance to value over time ticks; returns 0 when no music is current.
 * Nothing in the tree calls it. */
int music_balance_shift_fade(int value, int time) {
    if (g_music.state.current_music != 0) {
        suzuki_smd_set_balance_shift(g_music.state.current_music, value, time);
        return 1;
    }
    return 0;
}

/* Requests a tune (music_tune_e) unless one is pending or playing: the
 * scenario music fades out and music_tunes_update starts the tune once
 * it has stopped. */
int music_tune_play(int tune) {
    if (tune != 0) {
        if (g_music.slots.tune == 0) {
            music_volume_target_set(0, 120);
            g_music.slots.tune = tune | MUSIC_TUNE_PENDING;
        }
    }
    return 1;
}

/* Cancels a pending tune, or fades a playing one out to volume 0 over time
 * ticks (0: at once). */
int music_tune_fade_out(int time) {
    int tune = g_music.slots.tune;

    if (tune != 0) {
        if ((tune & MUSIC_TUNE_PENDING) != 0) {
            g_music.slots.tune = 0;
        } else {
            suzuki_smd_set_music_volume(g_music.slots.music[1 + (tune & MUSIC_TUNE_ID_MASK)], 0, time);
        }
    }
    return 1;
}

/* Advances the play-tune state machine once per frame: a pending tune
 * (MUSIC_TUNE_PENDING) starts once the scenario music has stopped; a playing tune is
 * torn down once it ends and the scenario music is faded back in. The tune
 * at `tune_index` lives in music slot 2 + `tune_index`. */
s32 music_tunes_update(void) {
    s32 tune = g_music.slots.tune;
    s32 tune_index;

    if (tune & MUSIC_TUNE_PENDING) {
        suzuki_music_t** tune_handles;
        suzuki_music_t** handle;

        if (suzuki_smd_is_music_playing(g_music.state.current_music) != 0) {
            return 0;
        }
        tune_index = g_music.slots.tune & MUSIC_TUNE_ID_MASK;
        /* Drop MUSIC_TUNE_PENDING: the tune is now playing. */
        g_music.slots.tune &= MUSIC_TUNE_PLAYING_MASK;
        tune_handles = &g_music.slots.music[1];
        handle = &tune_handles[tune_index];
        if (*handle == 0) {
            *handle = suzuki_smd_create_music(g_music.slots.smd[1 + tune_index]);
        }
        suzuki_smd_reset_music(*handle, 0x7f, 0);
        return 1;
    }
    if (tune != 0) {
        suzuki_music_t** tune_handles;
        suzuki_music_t** handle;

        tune_index = tune & MUSIC_TUNE_ID_MASK;
        tune_handles = &g_music.slots.music[1];
        handle = &tune_handles[tune_index];
        if (suzuki_smd_is_music_playing(*handle) != 0) {
            return 0;
        }
        suzuki_smd_stop_music(*handle);
        if (*handle != 0) {
            suzuki_smd_unload_music(*handle);
        }
        *handle = 0;
        g_music.slots.tune = 0;
        return music_volume_target_set(0x7f, 0x78);
    }
    return 0;
}

/* Stop the current music, unload both scenario music slots and cancel a pending tune or silence a
 * playing one at once. */
void music_all_unload(void) {
    music_stop();
    music_scenario_slot_unload(1);
    music_scenario_slot_unload(2);
    music_tune_fade_out(0);
}

/* Return the music record loaded in slot (slots count from 1; an unloaded slot holds 0). */
suzuki_music_t* music_handle_get(int slot) {
    return g_music.slots.music[slot - 1];
}

/* Stop the current music and unload the slot it played from. */
void music_current_scenario_unload(void) {
    int slot = g_music.state.current_slot;

    music_stop();
    music_scenario_slot_unload(slot);
}

/* Replaces the playing scenario music: stops and unloads it, loads music
 * file music_id (music_id_e) into a free slot and starts it at full
 * volume. */
void music_track_set(int music_id) {
    int slot;

    if (g_music.state.current_smd != 0) {
        slot = g_music.state.current_slot;
        music_stop();
        music_scenario_slot_unload(slot);
    }
    slot = music_free_slot_load(music_id);
    if (slot != 0) {
        music_track_switch(slot, 0x7f, 0);
    }
}

/* Play a sound effect on the fixed SFX channels 6-7 (suzuki_sound_play_direct_sfx), which channel
 * stealing never takes; menus use it for their confirm, cancel and cursor sounds. */
void sound_sfx_play(int sound_id) {
    suzuki_sound_play_direct_sfx(sound_id);
}

/* Plays a sound on a free SFX channel pair, or on the oldest stealable one. */
void sound_free_channel_sfx_play(int sound_id) {
    suzuki_sound_play_sfx(sound_id);
}

/* Plays a background sound on SFX channels 4-5 and remembers its id in
 * g_sound_background_sfx_id, so sound_background_stop can
 * stop it and sound_background_replay can start it again. */
void sound_background_play(int sound_id) {
    g_sound_background_sfx_id = sound_id;
    suzuki_sound_play_background_sfx(sound_id);
}

/* Starts the remembered background sound again. */
void sound_background_replay(void) {
    int sound_id = g_sound_background_sfx_id;

    if (sound_id != 0) {
        suzuki_sound_play_background_sfx(sound_id);
    }
}

/* Sets the volume of the SFX channels playing sound_id: volume is the new
 * channel velocity, and 0 stops them (and forgets a remembered background
 * sound). */
void sound_sfx_volume_set(int sound_id, int volume) {
    if (g_sound_background_sfx_id == sound_id && volume == 0) {
        g_sound_background_sfx_id = 0;
    }
    suzuki_sound_set_sfx_volume(sound_id, volume);
}

/* Forgets the remembered background sound and silences every SFX channel.
 * It leaves the music alone. */
void sound_all_sfx_stop(void) {
    g_sound_background_sfx_id = 0;
    suzuki_sound_stop_all_sfx();
}

/* Stops the SFX channels playing sound_id, forgetting it if it is the
 * remembered background sound. */
void sound_sfx_stop(int sound_id) {
    if (sound_id == g_sound_background_sfx_id) {
        g_sound_background_sfx_id = 0;
    }
    suzuki_sound_stop_sfx_channels(sound_id);
}

/* Stops the remembered background sound but keeps its id, so
 * sound_background_replay can restart it. */
void sound_background_stop(void) {
    int sound_id = g_sound_background_sfx_id;

    if (sound_id != 0) {
        suzuki_sound_stop_sfx_channels(sound_id);
    }
}

/* Sector-rounded read counts. The final SYSTEM.SED and ENV.SED sectors each
 * write 0x400 zero bytes into the following startup state. */
#define WAVESET_WD_SIZE 0x79000
#define SYSTEM_SED_SIZE 0x2800
#define ENV_SED_SIZE    0x1000

/* Start the sound driver and load the resident sound data: WAVESET.WD is read to
 * the low overlay address and uploaded to SPU RAM, SYSTEM.SED and ENV.SED are
 * appended to the driver's sound resources, the music slots are cleared, and the
 * five resident tunes (music files 45..49) are read into SMD slots 3..7. */
void sound_init(void) {
    s32 i;

    suzuki_sound_init(0);
    heap_smd_table_clear();
    suzuki_sound_load_waveset(
        file_buffer_load(DISC_SECTOR_SOUND_WAVESET_WD, WAVESET_WD_SIZE, g_heap_low_overlay_load_address));
    suzuki_sound_append_resource(
        file_buffer_load(DISC_SECTOR_SOUND_SYSTEM_SED, SYSTEM_SED_SIZE, g_sound_system_sed_data));
    suzuki_sound_append_resource(file_buffer_load(DISC_SECTOR_SOUND_ENV_SED, ENV_SED_SIZE, g_sound_env_sed_data));

    g_music.state.current_smd = 0;
    g_music.slots.tune = 0;
    g_music.slots.loading_slot = 0;
    for (i = 6; i >= 0; i--) {
        g_music.slots.music[i] = 0;
    }

    g_sound_background_sfx_id = 0;
    g_music.slots.smd[2] = file_smd_load(g_music_files[45].sector, g_music_files[45].size);
    g_music.slots.smd[3] = file_smd_load(g_music_files[46].sector, g_music_files[46].size);
    g_music.slots.smd[4] = file_smd_load(g_music_files[47].sector, g_music_files[47].size);
    g_music.slots.smd[5] = file_smd_load(g_music_files[48].sector, g_music_files[48].size);
    g_music.slots.smd[6] = file_smd_load(g_music_files[49].sector, g_music_files[49].size);
}
