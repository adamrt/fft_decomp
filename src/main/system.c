/* The resident executable's system services: VSync play-time accounting and the installed drawing
 * and CD callbacks, scenario music loading, playback and parameters with the resident tunes and sound
 * effects, the game and music heaps, and blocking disc reads with heap-backed load helpers. */

#include "fft/sys/system.h"
#include "fft/sys/file.h"
#include "fft/sys/memory.h"
#include "fft/sys/music.h"
#include "fft/sys/sound.h"
#include "fft/sys/suzuki.h"
#include "fft/world/save.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/types.h"

static void* alloc_smd_block(u32 size);
static s32 free_smd_block(void* ptr);
static void clear_smd_table(void);
static suzuki_smd_header_t* load_smd_file(int sector, u32 size);
static void* load_file_into_buffer(int sector, u32 size, void* destination);

/* callbacks */

s32 g_save_play_time_frames;
s32 g_save_play_time_seconds;
s32 g_save_play_time_minutes;
s32 g_save_play_time_hours;

/* VSync callback. Draws one rand() per frame, advances the play-time clock at 60
 * frames a second (the hours stop at 1000) and counts
 * g_system_wait_frames. */
void system_vsync_callback(void) {
    rand();

    g_save_play_time_frames++;
    if (g_save_play_time_frames >= 60) {
        g_save_play_time_frames = 0;
        g_save_play_time_seconds++;
        if (g_save_play_time_seconds >= 60) {
            g_save_play_time_seconds = 0;
            g_save_play_time_minutes++;
            if (g_save_play_time_minutes >= 60) {
                g_save_play_time_minutes = 0;
                if (g_save_play_time_hours < 1000) {
                    g_save_play_time_hours++;
                }
            }
        }
    }

    g_system_wait_frames++;
}

/* Empty DrawSync callback, installed by boot_run_startup. */
void system_draw_sync_callback(void) {
}

/* Empty CdReadyCallback handler, installed by boot_run_startup; the CdInit
 * that init_memory_and_cd runs right after replaces it, so it never
 * runs. */
void file_cd_ready_callback(u8 event, u8* result) {
}

/* Empty CdReadCallback handler, installed by boot_run_startup and replaced
 * by CdInit right after, like the ready callback. */
void file_cd_read_callback(u8 event, u8* result) {
}

/* sound requests */

extern u8 g_sound_system_sed_data[0x2400]; /* SYSTEM_SED_SIZE less the 0x400 its last sector overruns */
extern u8 g_sound_env_sed_data[0xc00];     /* ENV_SED_SIZE less the 0x400 the last sector spills */
extern s32 g_sound_background_sfx_id;      /* the background sound sound_play_background started, 0 for none */

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
static file_extent_t s_music_files[MUSIC_COUNT] = {
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
s32 music_load_into_slot(s32 music_id, s32 slot) {
    suzuki_smd_header_t* smd;

    if (g_music.slots.smd[slot - 1] == 0) {
        smd = load_smd_file(s_music_files[music_id].sector, s_music_files[music_id].size);
        g_music.slots.smd[slot - 1] = smd;
        g_music.slots.music[slot - 1] = suzuki_smd_create_music(smd);
        return slot;
    }
    return 0;
}

/* music_load_into_slot into the first free scenario slot (1 or
 * 2); returns 0 when both are taken. */
int music_load_into_free_slot(int music_id) {
    int i;
    suzuki_smd_header_t* smd;

    for (i = 0; i < 2; i++) {
        if (g_music.slots.smd[i] == 0) {
            smd = load_smd_file(s_music_files[music_id].sector, s_music_files[music_id].size);
            g_music.slots.smd[i] = smd;
            g_music.slots.music[i] = suzuki_smd_create_music(smd);
            return i + 1;
        }
    }
    return 0;
}

/* Always 0. Callers poll it after opening music and start the track once it returns 0. */
int music_is_loading(void) {
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
static s32 start_music_load_into_slot(s32 music_id, s32 slot) {
    if (g_music.slots.smd[slot - 1] == 0) {
        suzuki_smd_header_t* data = alloc_smd_block(s_music_files[music_id].size);

        if (data != 0
            && file_request_read_bytes(s_music_files[music_id].sector, s_music_files[music_id].size, data) == 0) {
            g_music.slots.smd[slot - 1] = data;
            set_opened_music_slot(slot);
            return slot;
        }
        system_handle_error(SYSTEM_ERROR_SMD_LOAD);
        return 0;
    }
    return 0;
}

/* Starts the asynchronous load of music file `music_id` (music_id_e) into
 * the first free scenario music slot (1 or 2) and returns that slot, or 0
 * when both are taken or the load fails (reporting SYSTEM_ERROR_SMD_LOAD;
 * as in start_music_load_into_slot, a read that cannot start leaks its
 * SMD block). music_poll_scenario_load creates the music record once
 * the file has arrived. */
int music_start_load_into_free_slot(int music_id) {
    int i;
    suzuki_smd_header_t* buffer;

    for (i = 0; i < 2; i++) {
        if (g_music.slots.smd[i] == 0) {
            buffer = alloc_smd_block(s_music_files[music_id].size);
            if (buffer != 0) {
                if (file_request_read_bytes(s_music_files[music_id].sector, s_music_files[music_id].size, buffer)
                    == 0) {
                    g_music.slots.smd[i] = buffer;
                    g_music.slots.loading_slot = i + 1;
                } else {
                    system_handle_error(SYSTEM_ERROR_SMD_LOAD);
                    return 0;
                }
            } else {
                system_handle_error(SYSTEM_ERROR_SMD_LOAD);
                return 0;
            }
            return i + 1;
        }
    }
    return 0;
}

/* Finish the asynchronous scenario load and create the music record for
 * loading_slot (1 or 2). */
int music_poll_scenario_load(void) {
    if (g_music.slots.loading_slot == 0) {
        return 0;
    }
    if (file_get_load_state() != 0) {
        return 1;
    }
    g_music.slots.music[g_music.slots.loading_slot - 1]
        = suzuki_smd_create_music(g_music.slots.smd[g_music.slots.loading_slot - 1]);
    g_music.slots.loading_slot = 0;
    return 0;
}

/* Blocking form of music_start_load_into_free_slot: polls the loader each
 * frame until the music record exists. Returns the slot, or 0 when no load
 * started. */
static s32 load_scenario_music_and_wait(s32 music_id) {
    s32 result = music_start_load_into_free_slot(music_id);

    if (result != 0) {
        while (music_poll_scenario_load() != 0) {
            VSync(0);
            system_frame_stub();
            file_poll_load(&g_file_cd_state);
        }
    }
    return result;
}

/* Frees the music record and SMD data of a slot. It does not clear the
 * current-music state when the slot is the playing one. */
s32 music_unload_scenario_slot(s32 slot) {
    if (g_music.slots.smd[slot - 1] != 0) {
        suzuki_smd_unload_music(g_music.slots.music[slot - 1]);
        g_music.slots.music[slot - 1] = 0;
        free_smd_block(g_music.slots.smd[slot - 1]);
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
suzuki_music_t* music_play_slot(s32 slot, s32 volume, s32 fade) {
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
static int get_current_music_slot(void) {
    return g_music.state.current_slot;
}

/* Fades the current music to volume over time ticks without remembering
 * the volume. */
int music_set_volume(int volume, int time) {
    if (g_music.state.current_music != 0) {
        suzuki_smd_set_music_volume(g_music.state.current_music, volume, time);
        return 1;
    }
    return 0;
}

/* Fades the current music back to the remembered volume over time ticks. */
int music_restore_volume(int time) {
    if (g_music.state.current_music != 0) {
        suzuki_smd_set_music_volume(g_music.state.current_music, g_music.state.target_volume, time);
        return 1;
    }
    return 0;
}

/* Fades the current music to volume over time ticks and remembers the
 * volume for music_restore_volume. */
int music_set_target_volume(int volume, int time) {
    if (g_music.state.current_music != 0) {
        g_music.state.target_volume = volume;
        suzuki_smd_set_music_volume(g_music.state.current_music, volume, time);
        return 1;
    }
    return 0;
}

/* Change the current music's tempo scale to value over time ticks; returns 0 when no music is
 * current. Nothing in the tree calls it. */
static int fade_music_tempo_scale(int value, int time) {
    if (g_music.state.current_music != 0) {
        suzuki_smd_set_tempo_scale(g_music.state.current_music, value, time);
        return 1;
    }
    return 0;
}

/* Shift the current music's pitch to value over time ticks; returns 0 when no music is current.
 * Nothing in the tree calls it. */
static int fade_music_pitch_shift(int value, int time) {
    if (g_music.state.current_music != 0) {
        suzuki_smd_set_pitch_shift(g_music.state.current_music, value, time);
        return 1;
    }
    return 0;
}

/* Shift the current music's balance to value over time ticks; returns 0 when no music is current.
 * Nothing in the tree calls it. */
static int fade_music_balance_shift(int value, int time) {
    if (g_music.state.current_music != 0) {
        suzuki_smd_set_balance_shift(g_music.state.current_music, value, time);
        return 1;
    }
    return 0;
}

/* Requests a tune (music_tune_e) unless one is pending or playing: the
 * scenario music fades out and music_update_tunes starts the tune once
 * it has stopped. */
int music_play_tune(int tune) {
    if (tune != 0) {
        if (g_music.slots.tune == 0) {
            music_set_target_volume(0, 120);
            g_music.slots.tune = tune | MUSIC_TUNE_PENDING;
        }
    }
    return 1;
}

/* Cancels a pending tune, or fades a playing one out to volume 0 over time
 * ticks (0: at once). */
int music_fade_out_tune(int time) {
    int tune = g_music.slots.tune;

    if (tune != 0) {
        if (tune & MUSIC_TUNE_PENDING) {
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
s32 music_update_tunes(void) {
    s32 tune = g_music.slots.tune;
    s32 tune_index;

    if (tune & MUSIC_TUNE_PENDING) {
        suzuki_music_t** tune_handles;
        suzuki_music_t** handle;

        if (suzuki_smd_is_music_playing(g_music.state.current_music)) {
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
        if (suzuki_smd_is_music_playing(*handle)) {
            return 0;
        }
        suzuki_smd_stop_music(*handle);
        if (*handle != 0) {
            suzuki_smd_unload_music(*handle);
        }
        *handle = 0;
        g_music.slots.tune = 0;
        return music_set_target_volume(0x7f, 0x78);
    }
    return 0;
}

/* Stop the current music, unload both scenario music slots and cancel a pending tune or silence a
 * playing one at once. */
void music_unload_all(void) {
    music_stop();
    music_unload_scenario_slot(1);
    music_unload_scenario_slot(2);
    music_fade_out_tune(0);
}

/* Return the music record loaded in slot (slots count from 1; an unloaded slot holds 0). */
static suzuki_music_t* get_music_by_slot(int slot) {
    return g_music.slots.music[slot - 1];
}

/* Stop the current music and unload the slot it played from. */
void music_unload_current_scenario(void) {
    int slot = g_music.state.current_slot;

    music_stop();
    music_unload_scenario_slot(slot);
}

/* Replaces the playing scenario music: stops and unloads it, loads music
 * file music_id (music_id_e) into a free slot and starts it at full
 * volume. */
void music_set_track(int music_id) {
    int slot;

    if (g_music.state.current_smd != 0) {
        slot = g_music.state.current_slot;
        music_stop();
        music_unload_scenario_slot(slot);
    }
    slot = music_load_into_free_slot(music_id);
    if (slot != 0) {
        music_play_slot(slot, 0x7f, 0);
    }
}

/* Play a sound effect on the fixed SFX channels 6-7 (suzuki_sound_play_direct_sfx), which channel
 * stealing never takes; menus use it for their confirm, cancel and cursor sounds. */
void sound_play_sfx(int sound_id) {
    suzuki_sound_play_direct_sfx(sound_id);
}

/* Plays a sound on a free SFX channel pair, or on the oldest stealable one. */
void sound_play_sfx_on_free_channel(int sound_id) {
    suzuki_sound_play_sfx(sound_id);
}

/* Plays a background sound on SFX channels 4-5 and remembers its id in
 * g_sound_background_sfx_id, so sound_stop_background can
 * stop it and sound_replay_background can start it again. */
void sound_play_background(int sound_id) {
    g_sound_background_sfx_id = sound_id;
    suzuki_sound_play_background_sfx(sound_id);
}

/* Starts the remembered background sound again. */
void sound_replay_background(void) {
    int sound_id = g_sound_background_sfx_id;

    if (sound_id != 0) {
        suzuki_sound_play_background_sfx(sound_id);
    }
}

/* Sets the volume of the SFX channels playing sound_id: volume is the new
 * channel velocity, and 0 stops them (and forgets a remembered background
 * sound). */
void sound_set_sfx_volume(int sound_id, int volume) {
    if (g_sound_background_sfx_id == sound_id && volume == 0) {
        g_sound_background_sfx_id = 0;
    }
    suzuki_sound_set_sfx_volume(sound_id, volume);
}

/* Forgets the remembered background sound and silences every SFX channel.
 * It leaves the music alone. */
void sound_stop_all_sfx(void) {
    g_sound_background_sfx_id = 0;
    suzuki_sound_stop_all_sfx();
}

/* Stops the SFX channels playing sound_id, forgetting it if it is the
 * remembered background sound. */
void sound_stop_sfx(int sound_id) {
    if (sound_id == g_sound_background_sfx_id) {
        g_sound_background_sfx_id = 0;
    }
    suzuki_sound_stop_sfx_channels(sound_id);
}

/* Stops the remembered background sound but keeps its id, so
 * sound_replay_background can restart it. */
void sound_stop_background(void) {
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
    clear_smd_table();
    suzuki_sound_load_waveset(
        load_file_into_buffer(DISC_SECTOR_SOUND_WAVESET_WD, WAVESET_WD_SIZE, g_heap_low_overlay_load_address));
    suzuki_sound_append_resource(
        load_file_into_buffer(DISC_SECTOR_SOUND_SYSTEM_SED, SYSTEM_SED_SIZE, g_sound_system_sed_data));
    suzuki_sound_append_resource(load_file_into_buffer(DISC_SECTOR_SOUND_ENV_SED, ENV_SED_SIZE, g_sound_env_sed_data));

    g_music.state.current_smd = 0;
    g_music.slots.tune = 0;
    g_music.slots.loading_slot = 0;
    for (i = 6; i >= 0; i--) {
        g_music.slots.music[i] = 0;
    }

    g_sound_background_sfx_id = 0;
    g_music.slots.smd[2] = load_smd_file(s_music_files[45].sector, s_music_files[45].size);
    g_music.slots.smd[3] = load_smd_file(s_music_files[46].sector, s_music_files[46].size);
    g_music.slots.smd[4] = load_smd_file(s_music_files[47].sector, s_music_files[47].size);
    g_music.slots.smd[5] = load_smd_file(s_music_files[48].sector, s_music_files[48].size);
    g_music.slots.smd[6] = load_smd_file(s_music_files[49].sector, s_music_files[49].size);
}

/* heaps */

extern u8 g_heap_game_allocator_table[HEAP_GAME_PAGE_COUNT];
extern u8 g_heap_smd_allocator_table[HEAP_SMD_PAGE_COUNT];
extern u8 g_heap_smd_base[HEAP_SMD_PAGE_COUNT * HEAP_PAGE_SIZE];

/* Allocate size bytes from the music heap: 16 pages of 0x800 bytes at
 * g_heap_smd_base, tagged in g_heap_smd_allocator_table. The algorithm
 * is heap_alloc's; returns 0 when no run of free pages is long enough. */
static void* alloc_smd_block(u32 size) {
    u32 run = 0;
    u32 needed = size >> HEAP_PAGE_SHIFT;
    u32 tag = 1;
    u32 index;
    u32 start;

    if (size & (HEAP_PAGE_SIZE - 1)) {
        needed++;
    }
    for (index = 0; index < HEAP_SMD_PAGE_COUNT; index++) {
        u32 cell;

        cell = g_heap_smd_allocator_table[index];
        if (cell == 0) {
            if (run == 0) {
                start = index;
            }
            run++;
            if (run >= needed) {
                break;
            }
        } else {
            if (cell >= tag) {
                tag = cell + 1;
            }
            run = 0;
        }
    }
    if (run >= needed) {
        for (index = 0; index < run; index++) {
            g_heap_smd_allocator_table[start + index] = tag;
        }
        return &g_heap_smd_base[start << HEAP_PAGE_SHIFT];
    }
    return 0;
}

/* Free the music-heap block that starts at ptr; the rules are heap_free's. */
static s32 free_smd_block(void* ptr) {
    u32 index = (u32)((u8*)ptr - g_heap_smd_base) >> HEAP_PAGE_SHIFT;
    s32 tag = g_heap_smd_allocator_table[index];
    u8* entry;

    if (((index == 0) | (tag != g_heap_smd_allocator_table[index - 1])) == 0) {
        return 0;
    }
    entry = &g_heap_smd_allocator_table[index];
    do {
        *entry++ = 0;
    } while (*entry == tag);
    return 1;
}

/* Mark every page of the music (SMD) heap free. */
static void clear_smd_table(void) {
    int index = HEAP_SMD_PAGE_COUNT - 1;
    u8* entry = &g_heap_smd_allocator_table[HEAP_SMD_PAGE_COUNT - 1];

    for (; index >= 0; index--, entry--) {
        *entry = 0;
    }
}

/* Allocate size bytes from the game heap: 64 pages of 0x800 bytes starting at
 * g_heap_high_overlay_load_address, one tag byte per page in
 * g_heap_game_allocator_table (0 = free).
 *
 * First fit over whole pages. The new block's tag is one more than the highest
 * tag met before the block, so it differs from the block below it, but the block
 * above can hold the same tag; heap_free tells blocks apart by tag alone.
 * Returns 0 when no run of free pages is long enough. */
void* heap_alloc(u32 size) {
    u32 run = 0;
    u32 needed = size >> HEAP_PAGE_SHIFT;
    u32 tag = 1;
    u32 index;
    u32 start;

    if (size & (HEAP_PAGE_SIZE - 1)) {
        needed++;
    }
    for (index = 0; index < HEAP_GAME_PAGE_COUNT; index++) {
        u32 cell = g_heap_game_allocator_table[index];

        if (cell == 0) {
            if (run == 0) {
                start = index;
            }
            run++;
            if (run >= needed) {
                break;
            }
        } else {
            if (cell >= tag) {
                tag = cell + 1;
            }
            run = 0;
        }
    }
    if (run >= needed) {
        for (index = 0; index < run; index++) {
            g_heap_game_allocator_table[start + index] = tag;
        }
        return (start << HEAP_PAGE_SHIFT) + g_heap_high_overlay_load_address;
    }
    return 0;
}

/* Reserve free heap pages at a fixed address, using the lowest unused tag.
 * Only the starting address is bounds-checked; the page range may extend
 * beyond the heap. Returns the page-aligned address, or 0 on failure. */
void* heap_reserve_at(void* allocation, u32 size) {
    u32 addr = (u32)allocation;
    u32 base = (u32)g_heap_high_overlay_load_address;
    s32 start;
    u32 nblocks;
    s32 id = 1;
    u32 i;
    u32 j;

    start = ((u8*)allocation - g_heap_high_overlay_load_address) / HEAP_PAGE_SIZE;
    nblocks = size >> HEAP_PAGE_SHIFT;
    if (size & (HEAP_PAGE_SIZE - 1)) {
        nblocks += 1;
    }
    if (addr < base) {
        return 0;
    }
    if (addr < base + HEAP_GAME_PAGE_COUNT * HEAP_PAGE_SIZE) {
        do {
            for (i = 0; i < HEAP_GAME_PAGE_COUNT; i++) {
                if (g_heap_game_allocator_table[i] == id) {
                    id += 1;
                    break;
                }
            }
        } while (i != HEAP_GAME_PAGE_COUNT);

        for (i = start; i < start + nblocks; i++) {
            if (g_heap_game_allocator_table[i] != 0) {
                break;
            }
        }
        if (i == start + nblocks) {
            for (j = 0; j < nblocks; j++) {
                g_heap_game_allocator_table[start + j] = id;
            }
            return ((start << HEAP_PAGE_SHIFT) + g_heap_high_overlay_load_address);
        }
    }
    return 0;
}

/* Free the game-heap block that starts at pointer: clear its page tags from that
 * page on while they repeat the block's tag. Returns 1, or 0 without freeing when
 * the page below holds the same tag (pointer is taken to be inside a block).
 *
 * A block directly above with the same tag (see heap_alloc) is freed with
 * it. For page 0 the test also reads the byte before the table. */
s32 heap_free(void* pointer) {
    u32 page_index = (u32)((u8*)pointer - g_heap_high_overlay_load_address) >> HEAP_PAGE_SHIFT;
    s32 allocation_id = g_heap_game_allocator_table[page_index];
    s32 previous_id = g_heap_game_allocator_table[page_index - 1];

    if ((page_index < 1) | (allocation_id != previous_id)) {
        u8* entry = &g_heap_game_allocator_table[page_index];

        do {
            *entry++ = 0;
        } while (*entry == allocation_id);
        return 1;
    }
    return 0;
}

/* Mark every page of the game heap free; startup and each later game reset call it. */
void heap_clear_game_table(void) {
    int index = HEAP_GAME_PAGE_COUNT - 1;
    u8* entry = &g_heap_game_allocator_table[HEAP_GAME_PAGE_COUNT - 1];

    for (; index >= 0; index--, entry--) {
        *entry = 0;
    }
}

/* file loading */

/*
 * Start an asynchronous disc read through the shared file header.
 *
 * Overlay callers pass a byte count; the lower-level builder takes sectors.
 * Discard any partial sector rather than rounding the request up.
 */
s32 file_request_read_bytes(s32 sector, u32 size, void* destination) {
    return file_request_read_quiet(&g_file_cd_state, sector, size >> 11, destination);
}

/* Return the shared CD descriptor's load state: nonzero while a read through g_file_cd_state
 * is in progress. */
int file_get_load_state(void) {
    return g_file_cd_state.state;
}

/* Blocking read of a sound file into a new block of the music heap. size is in
 * bytes and only whole sectors are read. A full heap reports
 * SYSTEM_ERROR_SMD_LOAD and returns 0; the loader then only seeks, because
 * it skips the read for a null destination. */
static suzuki_smd_header_t* load_smd_file(int sector, u32 size) {
    suzuki_smd_header_t* buffer = alloc_smd_block(size);

    if (buffer == 0) {
        system_handle_error(SYSTEM_ERROR_SMD_LOAD);
    }
    while (file_request_read_quiet(&g_file_cd_state, sector, size >> 11, buffer) != 0) {
        VSync(0);
        system_frame_stub();
        file_poll_load(&g_file_cd_state);
    }
    while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
        VSync(0);
        system_frame_stub();
        file_poll_load(&g_file_cd_state);
    }
    return buffer;
}

/* Allocate size bytes from the game heap and read size >> 11 whole sectors,
 * blocking until the read ends. The caller must include sector rounding in
 * size; a partial final sector is not read. load_smd_file uses the same read
 * loop with the separate music heap. */
static void* load_into_heap(s32 sector, u32 size) {
    void* destination = heap_alloc(size);

    if (destination == 0) {
        system_handle_error(SYSTEM_ERROR_HEAP_ALLOC);
    }
    while (file_request_read_quiet(&g_file_cd_state, sector, size >> 11, destination) != 0) {
        VSync(0);
        system_frame_stub();
        file_poll_load(&g_file_cd_state);
    }
    while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
        VSync(0);
        system_frame_stub();
        file_poll_load(&g_file_cd_state);
    }
    return destination;
}

/* Blocking read from a disc sector to destination without the "Now Loading"
 * message. size is in bytes and only whole sectors (size >> 11) are read. Waits
 * for the shared loader to be idle, then polls it once per frame until the read
 * ends. Always returns destination. */
static void* load_file_into_buffer(int sector, u32 size, void* destination) {
    while (file_request_read_quiet(&g_file_cd_state, sector, size >> 11, destination) != 0) {
        VSync(0);
        system_frame_stub();
        file_poll_load(&g_file_cd_state);
    }
    while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
        VSync(0);
        system_frame_stub();
        file_poll_load(&g_file_cd_state);
    }
    return destination;
}

/* Blocking load_file_into_buffer that reports SYSTEM_ERROR_FILE_LOAD on a null result;
 * since that loader returns destination, only a null destination trips it. */
void* file_load_into_buffer_checked(int sector, int size, void* destination) {
    void* result = load_file_into_buffer(sector, size, destination);

    if (result == 0) {
        system_handle_error(SYSTEM_ERROR_FILE_LOAD);
    }
    return result;
}

/* Blocking load into a new game-heap block that also reports SYSTEM_ERROR_FILE_LOAD when no
 * block was free (load_into_heap has already reported SYSTEM_ERROR_HEAP_ALLOC). */
void* file_load_into_heap_checked(int sector, int size) {
    void* result = load_into_heap(sector, size);

    if (result == 0) {
        system_handle_error(SYSTEM_ERROR_FILE_LOAD);
    }
    return result;
}

/* Free a game-heap block through heap_free, dropping its result. */
void heap_release_block(void* pointer) {
    heap_free(pointer);
}
