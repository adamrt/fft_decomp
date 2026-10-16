/* The new game display setter (a second copy of the display setter), display swaps and fades, vsync, draw sync and CD
 * callbacks, music loading, playback and parameters, tunes, and sound effects. */

#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/music.h"
#include "fft/save.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
#include "psx/types.h"

file_extent_t g_music_scenario_smd_files[45] = { { 85249, 10240 }, { 85254, 10240 }, { 85259, 6144 }, { 85262, 6144 },
    { 85265, 6144 }, { 85268, 6144 }, { 85271, 6144 }, { 85274, 10240 }, { 85279, 6144 }, { 85282, 4096 },
    { 85284, 6144 }, { 85287, 10240 }, { 85292, 10240 }, { 85297, 8192 }, { 85301, 14336 }, { 85308, 8192 },
    { 85312, 4096 }, { 85314, 8192 }, { 85318, 18432 }, { 85327, 16384 }, { 85335, 2048 }, { 85336, 2048 },
    { 85337, 2048 }, { 85338, 2048 }, { 85339, 2048 }, { 85340, 4096 }, { 85342, 6144 }, { 85345, 2048 },
    { 85346, 4096 }, { 85348, 4096 }, { 85350, 2048 }, { 85351, 4096 }, { 85353, 2048 }, { 85354, 2048 },
    { 85355, 4096 }, { 85357, 2048 }, { 85358, 4096 }, { 85360, 10240 }, { 85365, 8192 }, { 85369, 6144 },
    { 85372, 2048 }, { 85373, 2048 }, { 85374, 4096 }, { 85376, 2048 }, { 85377, 2048 } };
static file_extent_t g_music_permanent_smd_files[5]
    = { { 85378, 2048 }, { 85379, 2048 }, { 85380, 2048 }, { 85381, 2048 }, { 85382, 2048 } };

/* A second copy of gfx_set_display_draw in the original, for the Orbonne
 * caption fades. Set up double-buffered drawing at width x height without
 * restarting the GPU: the second buffer sits 240 lines below the first (a
 * 480-line mode uses one), both draw environments clear to the given color,
 * and the display is masked until the caller turns it on. image_rect is not
 * used. */
static void new_game_set_display_draw(int width, int height, int projection, u8 red, u8 green, u8 blue) {
    RECT image_rect = { 0, 0, 1024, 512 };
    int second_y = height == 480 ? 0 : 240;

    SetDispMask(0);
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(projection);

    SetDefDrawEnv(&g_gfx_draw_envs[0], 0, 0, width, height);
    SetDefDispEnv(&g_gfx_display_envs[0], 0, second_y, width, height);
    SetDefDrawEnv(&g_gfx_draw_envs[1], 0, second_y, width, height);
    SetDefDispEnv(&g_gfx_display_envs[1], 0, 0, width, height);

    g_gfx_draw_envs[0].dtd = g_gfx_draw_envs[1].dtd = 0;
    g_gfx_draw_envs[0].isbg = g_gfx_draw_envs[1].isbg = 1;
    setRGB0(&g_gfx_draw_envs[0], red, green, blue);
    setRGB0(&g_gfx_draw_envs[1], red, green, blue);

    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);
}

/* Present one frame for the Orbonne caption fades: swap the display buffers after VSync, draw otag
 * and flush the debug print stream. Returns VSync's result. */
static int new_game_present_caption_frame(u32* otag) {
    /* Matching constraint: the retail function reserves one unused RECT. */
    RECT matching_stack_space;
    int elapsed;

    g_gfx_screen_polarity = g_gfx_screen_polarity == 0;
    DrawSync(0);
    elapsed = VSync(0);

    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);
    DrawOTag(otag);
    FntFlush(-1);
    return elapsed;
}

/* Fade in a 256x24 caption strip centered on a black 256x240 screen.
 *
 * The strip is the texture at VRAM (896, 256) with its CLUT at (896, 287), drawn
 * at y 116. A full-screen subtractive quad steps from 248 to 0 in 32 frames, each
 * presented with new_game_present_caption_frame. No function in the tree calls this
 * routine or new_game_fade_out_orbonne_caption. */
void new_game_fade_in_orbonne_caption(void) {
    POLY_F4 fade_polygons[2];
    DR_MODE draw_modes[2];
    POLY_FT4 title_polygons[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* current_otag;
    s16 intensity = 248;

    new_game_set_display_draw(256, 240, 512, 0, 0, 0);

    SetPolyF4(&fade_polygons[0]);
    setRGB0(&fade_polygons[0], 0, 0, 0);
    setXY4(&fade_polygons[0], 0, 0, 256, 0, 0, 256, 256, 256);
    SetSemiTrans(&fade_polygons[0], 1);
    fade_polygons[1] = fade_polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &image_rect);
    draw_modes[1] = draw_modes[0];

    setPolyFT4(&title_polygons[0]);
    setRGB0(&title_polygons[0], 128, 128, 128);
    setXY4(&title_polygons[0], 0, 116, 255, 116, 0, 140, 255, 140);
    setUV4(&title_polygons[0], 0, 0, 255, 0, 0, 24, 255, 24);
    SetSemiTrans(&title_polygons[0], 0);
    title_polygons[0].tpage = GetTPage(0, 0, 896, 256);
    title_polygons[0].clut = GetClut(896, 287);
    title_polygons[1] = title_polygons[0];

    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 256;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
    SetDispMask(1);

    while (intensity >= 0) {
        current_otag = otags[g_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&g_gfx_screen_polarity[fade_polygons], intensity, intensity, intensity);
        AddPrim(current_otag, &fade_polygons[g_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_gfx_screen_polarity]);
        AddPrim(current_otag, &title_polygons[g_gfx_screen_polarity]);
        new_game_present_caption_frame(current_otag);
        intensity -= 8;
    }

    DrawSync(0);
    DrawSync(0);
}

/* Fade out the caption strip new_game_fade_in_orbonne_caption shows: the
 * subtractive quad steps from 0 to 255 in 32 frames over the same strip, then
 * both display buffers are cleared. */
void new_game_fade_out_orbonne_caption(void) {
    POLY_F4 fade_polygons[2];
    DR_MODE draw_modes[2];
    POLY_FT4 title_polygons[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* current_otag;
    s16 intensity = 0;

    SetPolyF4(&fade_polygons[0]);
    setRGB0(&fade_polygons[0], 0, 0, 0);
    setXY4(&fade_polygons[0], 0, 0, 256, 0, 0, 256, 256, 256);
    SetSemiTrans(&fade_polygons[0], 1);
    fade_polygons[1] = fade_polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &image_rect);
    draw_modes[1] = draw_modes[0];

    setPolyFT4(&title_polygons[0]);
    setRGB0(&title_polygons[0], 128, 128, 128);
    setXY4(&title_polygons[0], 0, 116, 255, 116, 0, 140, 255, 140);
    setUV4(&title_polygons[0], 0, 0, 255, 0, 0, 24, 255, 24);
    SetSemiTrans(&title_polygons[0], 0);
    title_polygons[0].tpage = GetTPage(0, 0, 896, 256);
    title_polygons[0].clut = GetClut(896, 287);
    title_polygons[1] = title_polygons[0];

    while (intensity < 256) {
        if (intensity >= 248) {
            intensity = 255;
        }

        current_otag = otags[g_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&g_gfx_screen_polarity[fade_polygons], intensity, intensity, intensity);
        AddPrim(current_otag, &fade_polygons[g_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_gfx_screen_polarity]);
        AddPrim(current_otag, &title_polygons[g_gfx_screen_polarity]);
        new_game_present_caption_frame(current_otag);
        intensity += 8;
    }

    DrawSync(0);
    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 256;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
}

/* VSync callback. Draws one rand() per frame, advances the play-time clock at 60
 * frames a second (the hours stop at 1000) and counts
 * g_system_session_frames. */
void system_handle_vsync(void) {
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

    g_system_session_frames++;
}

/* Empty DrawSync callback, installed by boot_run_startup. */
void system_handle_draw_sync(void) {
}

/* Empty CdReadyCallback handler, installed by boot_run_startup; the CdInit
 * that file_reset_cdrom_cpu_ram runs right after replaces it, so it never
 * runs. */
void file_handle_cd_ready(u8 event, u8* result) {
}

/* Empty CdReadCallback handler, installed by boot_run_startup and replaced
 * by CdInit right after, like the ready callback. */
void file_handle_cd_read(u8 event, u8* result) {
}

/* Load scenario music `music_id` into scenario slot `slot` (1 or 2) when that slot is
 * free and return the slot; return 0 when it is taken. */
s32 music_load_into_slot(s32 music_id, s32 slot) {
    suzuki_smd_header_t* smd;

    if (g_music.slots.smd[slot - 1] == 0) {
        smd = file_load_smd(g_music_scenario_smd_files[music_id].sector, g_music_scenario_smd_files[music_id].size);
        g_music.slots.smd[slot - 1] = smd;
        g_music.slots.music[slot - 1] = suzuki_smd_create_music(smd);
        return slot;
    }
    return 0;
}

/* music_load_into_slot into the first free scenario slot (1 or
 * 2); returns 0 when both are taken. */
int music_open_into_free_slot(int music_id) {
    int i;
    suzuki_smd_header_t* smd;

    for (i = 0; i < 2; i++) {
        if (g_music.slots.smd[i] == 0) {
            smd = file_load_smd(g_music_scenario_smd_files[music_id].sector, g_music_scenario_smd_files[music_id].size);
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
s32 music_open_into_slot(s32 music_id, s32 slot) {
    if (g_music.slots.smd[slot - 1] == 0) {
        suzuki_smd_header_t* data = heap_alloc_smd(g_music_scenario_smd_files[music_id].size);

        if (data != 0
            && file_request_read_bytes(
                   g_music_scenario_smd_files[music_id].sector, g_music_scenario_smd_files[music_id].size, data)
                == 0) {
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
 * as in music_open_into_slot, a read that cannot start leaks its
 * SMD block). music_poll_scenario_load creates the music record once
 * the file has arrived. */
int music_start_load_into_free_slot(int music_id) {
    int i;
    suzuki_smd_header_t* buffer;

    for (i = 0; i < 2; i++) {
        if (g_music.slots.smd[i] == 0) {
            buffer = heap_alloc_smd(g_music_scenario_smd_files[music_id].size);
            if (buffer != 0) {
                if (file_request_read_bytes(
                        g_music_scenario_smd_files[music_id].sector, g_music_scenario_smd_files[music_id].size, buffer)
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
    if (file_is_still_loading() != 0) {
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
s32 music_wait_for_scenario_load(s32 music_id) {
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
        heap_free_smd(g_music.slots.smd[slot - 1]);
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
suzuki_music_t* music_switch_track(s32 slot, s32 volume, s32 fade) {
    music_stop();
    if (slot != 0) {
        if (g_music.slots.smd[slot - 1] != 0) {
            g_music.state.current_slot = slot;
            g_music.state.current_smd = g_music.slots.smd[slot - 1];
            g_music.state.current_music = g_music.slots.music[slot - 1];
            g_music.state.volume = volume;
            suzuki_smd_reset_music(g_music.state.current_music, volume, fade);
        }
        return g_music.state.current_music;
    }
    return 0;
}

/* Returns the slot of the current music (g_music.state.current_slot),
 * not a music id. */
int music_get_current_scenario(void) {
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
int music_set_volume_timed(int time) {
    if (g_music.state.current_music != 0) {
        suzuki_smd_set_music_volume(g_music.state.current_music, g_music.state.volume, time);
        return 1;
    }
    return 0;
}

/* Fades the current music to volume over time ticks and remembers the
 * volume for music_set_volume_timed. */
int music_set_target(int volume, int time) {
    if (g_music.state.current_music != 0) {
        g_music.state.volume = volume;
        suzuki_smd_set_music_volume(g_music.state.current_music, volume, time);
        return 1;
    }
    return 0;
}

/* Change the current music's tempo scale to value over time ticks; returns 0 when no music is
 * current. Nothing in the tree calls it. */
int music_set_tempo_scale_timed(int value, int time) {
    if (g_music.state.current_music != 0) {
        suzuki_smd_set_tempo_scale(g_music.state.current_music, value, time);
        return 1;
    }
    return 0;
}

/* Shift the current music's pitch to value over time ticks; returns 0 when no music is current.
 * Nothing in the tree calls it. */
int music_set_pitch_shift_timed(int value, int time) {
    if (g_music.state.current_music != 0) {
        suzuki_smd_set_pitch_shift(g_music.state.current_music, value, time);
        return 1;
    }
    return 0;
}

/* Shift the current music's balance to value over time ticks; returns 0 when no music is current.
 * Nothing in the tree calls it. */
int music_set_balance_shift_timed(int value, int time) {
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
            music_set_target(0, 120);
            g_music.slots.tune = tune | MUSIC_TUNE_PENDING;
        }
    }
    return 1;
}

/* Cancels a pending tune, or fades a playing one out to volume 0 over time
 * ticks (0: at once). */
int music_set_tune_volume(int time) {
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
s32 music_update_tunes(void) {
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
        return music_set_target(0x7f, 0x78);
    }
    return 0;
}

/* Stop the current music, unload both scenario music slots and cancel a pending tune or silence a
 * playing one at once. */
void music_unload_scenario_and_tunes(void) {
    music_stop();
    music_unload_scenario_slot(1);
    music_unload_scenario_slot(2);
    music_set_tune_volume(0);
}

/* Return the music record loaded in slot (slots count from 1; an unloaded slot holds 0). */
suzuki_music_t* music_get_handle(int slot) {
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
    slot = music_open_into_free_slot(music_id);
    if (slot != 0) {
        music_switch_track(slot, 0x7f, 0);
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
 * g_sound_weather_sfx_id, so sound_stop_weather can
 * stop it and sound_replay_background can start it again. */
void sound_play_background(int sound_id) {
    g_sound_weather_sfx_id = sound_id;
    suzuki_sound_play_background_sfx(sound_id);
}

/* Starts the remembered background sound again. */
void sound_replay_background(void) {
    int sound_id = g_sound_weather_sfx_id;

    if (sound_id != 0) {
        suzuki_sound_play_background_sfx(sound_id);
    }
}

/* Sets the volume of the SFX channels playing sound_id: volume is the new
 * channel velocity, and 0 stops them (and forgets a remembered background
 * sound). */
void sound_set_sfx_echo(int sound_id, int volume) {
    if (g_sound_weather_sfx_id == sound_id && volume == 0) {
        g_sound_weather_sfx_id = 0;
    }
    suzuki_sound_set_sfx_volume(sound_id, volume);
}

/* Forgets the remembered background sound and silences every SFX channel.
 * It leaves the music alone. */
void sound_stop_weather_music(void) {
    g_sound_weather_sfx_id = 0;
    suzuki_sound_stop_all_sfx();
}

/* Stops the SFX channels playing sound_id, forgetting it if it is the
 * remembered background sound. */
void sound_stop_sfx(int sound_id) {
    if (sound_id == g_sound_weather_sfx_id) {
        g_sound_weather_sfx_id = 0;
    }
    suzuki_sound_stop_sfx_channels(sound_id);
}

/* Stops the remembered background sound but keeps its id, so
 * sound_replay_background can restart it. */
void sound_stop_weather(void) {
    int sound_id = g_sound_weather_sfx_id;

    if (sound_id != 0) {
        suzuki_sound_stop_sfx_channels(sound_id);
    }
}

/* Byte budgets of the resident sound files. */
#define WAVESET_WD_SIZE 0x79000
#define SYSTEM_SED_SIZE 0x2800
#define ENV_SED_SIZE    0x1000

/* Start the sound driver and load the resident sound data: WAVESET.WD is read to
 * the low overlay address and uploaded to SPU RAM, SYSTEM.SED and ENV.SED are
 * appended to the driver's sound resources, the music slots are cleared, and the
 * five files of g_music_permanent_smd_files are read into SMD slots 3..7. */
void sound_open_generic_sfx(void) {
    s32 i;

    suzuki_sound_init(0);
    heap_clear_smd_table();
    suzuki_sound_load_waveset(
        file_load_to_address(DISC_SECTOR_SOUND_WAVESET_WD, WAVESET_WD_SIZE, g_heap_low_overlay_load_address));
    suzuki_sound_append_resource(
        file_load_to_address(DISC_SECTOR_SOUND_SYSTEM_SED, SYSTEM_SED_SIZE, g_sound_system_sed_data));
    suzuki_sound_append_resource(file_load_to_address(DISC_SECTOR_SOUND_ENV_SED, ENV_SED_SIZE, g_sound_env_sed_data));

    g_music.state.current_smd = 0;
    g_music.slots.tune = 0;
    g_music.slots.loading_slot = 0;
    for (i = 6; i >= 0; i--) {
        g_music.slots.music[i] = 0;
    }

    g_sound_weather_sfx_id = 0;
    g_music.slots.smd[2] = file_load_smd(g_music_permanent_smd_files[0].sector, g_music_permanent_smd_files[0].size);
    g_music.slots.smd[3] = file_load_smd(g_music_permanent_smd_files[1].sector, g_music_permanent_smd_files[1].size);
    g_music.slots.smd[4] = file_load_smd(g_music_permanent_smd_files[2].sector, g_music_permanent_smd_files[2].size);
    g_music.slots.smd[5] = file_load_smd(g_music_permanent_smd_files[3].sector, g_music_permanent_smd_files[3].size);
    g_music.slots.smd[6] = file_load_smd(g_music_permanent_smd_files[4].sector, g_music_permanent_smd_files[4].size);
}
