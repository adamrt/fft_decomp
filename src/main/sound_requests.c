/* The new game display setter (a second copy of the display setter), display swaps and fades, vsync, draw sync and CD
 * callbacks, music loading, playback and parameters, tunes, and sound effects. */

#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_gfx.h"
#include "fft/main_sound.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* The original wrote the body of main_set_display_draw a second time as main_set_display_draw_new_game. */
/* Set up double-buffered drawing at width x height without restarting the GPU:
 * the second buffer sits 240 lines below the first (a 480-line mode uses one),
 * both draw environments clear to the given color, and the display is masked
 * until the caller turns it on. image_rect is not used. */
static void main_set_display_draw_new_game(int width, int height, int projection, u8 red, u8 green, u8 blue) {
    RECT image_rect = { 0, 0, 1024, 512 };
    int second_y = height == 480 ? 0 : 240;

    SetDispMask(0);
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(projection);

    SetDefDrawEnv(&g_main_gfx_draw_envs[0], 0, 0, width, height);
    SetDefDispEnv(&g_main_gfx_display_envs[0], 0, second_y, width, height);
    SetDefDrawEnv(&g_main_gfx_draw_envs[1], 0, second_y, width, height);
    SetDefDispEnv(&g_main_gfx_display_envs[1], 0, 0, width, height);

    g_main_gfx_draw_envs[0].dtd = g_main_gfx_draw_envs[1].dtd = 0;
    g_main_gfx_draw_envs[0].isbg = g_main_gfx_draw_envs[1].isbg = 1;
    setRGB0(&g_main_gfx_draw_envs[0], red, green, blue);
    setRGB0(&g_main_gfx_draw_envs[1], red, green, blue);

    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);
}

/* Present one frame for the Orbonne caption fades: swap the display buffers after VSync, draw otag
 * and flush the debug print stream. Returns VSync's result. */
static int main_gfx_swap_display_area(u32* otag) {
    /* Matching constraint: the retail function reserves one unused RECT. */
    RECT matching_stack_space;
    int elapsed;

    g_main_gfx_screen_polarity = g_main_gfx_screen_polarity == 0;
    DrawSync(0);
    elapsed = VSync(0);

    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);
    DrawOTag(otag);
    FntFlush(-1);
    return elapsed;
}

/* Fade in a 256x24 caption strip centered on a black 256x240 screen.
 *
 * The strip is the texture at VRAM (896, 256) with its CLUT at (896, 287), drawn
 * at y 116. A full-screen subtractive quad steps from 248 to 0 in 32 frames, each
 * presented with main_gfx_swap_display_area. No function in the tree calls this
 * routine or main_gfx_fade_out_orbonne_monastery. */
void main_gfx_fade_in_orbonne_monastery(void) {
    POLY_F4 fade_polygons[2];
    DR_MODE draw_modes[2];
    POLY_FT4 title_polygons[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* current_otag;
    s16 intensity;

    intensity = 248;
    main_set_display_draw_new_game(256, 240, 512, 0, 0, 0);

    SetPolyF4(&fade_polygons[0]);
    setRGB0(&fade_polygons[0], 0, 0, 0);
    setXY4(&fade_polygons[0], 0, 0, 256, 0, 0, 256, 256, 256);
    SetSemiTrans(&fade_polygons[0], 1);
    fade_polygons[1] = fade_polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, 0x40, &image_rect);
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
        current_otag = otags[g_main_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&g_main_gfx_screen_polarity[fade_polygons], intensity, intensity, intensity);
        AddPrim(current_otag, &fade_polygons[g_main_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_main_gfx_screen_polarity]);
        AddPrim(current_otag, &title_polygons[g_main_gfx_screen_polarity]);
        main_gfx_swap_display_area(current_otag);
        intensity -= 8;
    }

    DrawSync(0);
    DrawSync(0);
}

/* Fade out the caption strip main_gfx_fade_in_orbonne_monastery shows: the
 * subtractive quad steps from 0 to 255 in 32 frames over the same strip, then
 * both display buffers are cleared. */
void main_gfx_fade_out_orbonne_monastery(void) {
    POLY_F4 fade_polygons[2];
    DR_MODE draw_modes[2];
    POLY_FT4 title_polygons[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* current_otag;
    s16 intensity;

    intensity = 0;

    SetPolyF4(&fade_polygons[0]);
    setRGB0(&fade_polygons[0], 0, 0, 0);
    setXY4(&fade_polygons[0], 0, 0, 256, 0, 0, 256, 256, 256);
    SetSemiTrans(&fade_polygons[0], 1);
    fade_polygons[1] = fade_polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, 0x40, &image_rect);
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

        current_otag = otags[g_main_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&g_main_gfx_screen_polarity[fade_polygons], intensity, intensity, intensity);
        AddPrim(current_otag, &fade_polygons[g_main_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_main_gfx_screen_polarity]);
        AddPrim(current_otag, &title_polygons[g_main_gfx_screen_polarity]);
        main_gfx_swap_display_area(current_otag);
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
 * g_main_system_session_frames. */
void main_system_handle_vsync_callback(void) {
    rand();

    g_main_system_play_time_frames++;
    if (g_main_system_play_time_frames >= 60) {
        g_main_system_play_time_frames = 0;
        g_main_system_play_time_seconds++;
        if (g_main_system_play_time_seconds >= 60) {
            g_main_system_play_time_seconds = 0;
            g_main_system_play_time_minutes++;
            if (g_main_system_play_time_minutes >= 60) {
                g_main_system_play_time_minutes = 0;
                if (g_main_system_play_time_hours < 1000) {
                    g_main_system_play_time_hours++;
                }
            }
        }
    }

    g_main_system_session_frames++;
}

/* Empty DrawSync callback, installed by main_boot_run_startup. */
void main_system_handle_draw_sync_callback(void) {
}

/* Empty CdReadyCallback handler, installed by main_boot_run_startup; the CdInit
 * that main_file_reset_cdrom_cpu_ram runs right after replaces it, so it never
 * runs. */
void main_file_handle_cd_ready_callback(u8 event, u8* result) {
}

/* Empty CdReadCallback handler, installed by main_boot_run_startup and replaced
 * by CdInit right after, like the ready callback. */
void main_file_handle_cd_read_callback(u8 event, u8* result) {
}

s32 main_sound_load_music_into_slot(s32 music_id, s32 slot) {
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
int main_sound_start_music_load_into_free_slot(int music_id) {
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
    s32 result = main_sound_start_music_load_into_free_slot(music_id);

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

/* Change the current music's tempo scale to value over time ticks; returns 0 when no music is
 * current. Nothing in the tree calls it. */
int main_sound_set_current_music_tempo_scale_timed(int value, int time) {
    if (g_main_sound_music.state.current_music != 0) {
        suzuki_smd_set_tempo_scale(g_main_sound_music.state.current_music, value, time);
        return 1;
    }
    return 0;
}

/* Shift the current music's pitch to value over time ticks; returns 0 when no music is current.
 * Nothing in the tree calls it. */
int main_sound_set_current_music_pitch_shift_timed(int value, int time) {
    if (g_main_sound_music.state.current_music != 0) {
        suzuki_smd_set_pitch_shift(g_main_sound_music.state.current_music, value, time);
        return 1;
    }
    return 0;
}

/* Shift the current music's balance to value over time ticks; returns 0 when no music is current.
 * Nothing in the tree calls it. */
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

/* Stop the current music, unload both scenario music slots and cancel a pending tune or silence a
 * playing one at once. */
void main_sound_unload_scenario_music_and_tunes(void) {
    main_sound_stop_current_music();
    main_sound_unload_scenario_mus(1);
    main_sound_unload_scenario_mus(2);
    main_sound_set_tune_volume(0);
}

/* Return the music record loaded in slot (slots count from 1; an unloaded slot holds 0). */
suzuki_music_t* main_sound_get_music_handle(int slot) {
    return g_main_sound_music.slots.music[slot - 1];
}

/* Stop the current music and unload the slot it played from. */
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

/* Play a sound effect on the fixed SFX channels 6-7 (suzuki_sound_play_direct_sfx), which channel
 * stealing never takes; menus use it for their confirm, cancel and cursor sounds. */
void main_sound_play_sfx(int sound_id) {
    suzuki_sound_play_direct_sfx(sound_id);
}

/* Plays a sound on a free SFX channel pair, or on the oldest stealable one. */
void main_sound_play_sfx_find_channel(int sound_id) {
    suzuki_sound_play_sfx(sound_id);
}

/* Plays a background sound on SFX channels 4-5 and remembers its id in
 * g_main_sound_weather_sfx_id, so main_sound_stop_current_weather_sfx can
 * stop it and main_sound_replay_bg_sfx can start it again. */
void main_sound_play_bg_sfx(int sound_id) {
    g_main_sound_weather_sfx_id = sound_id;
    suzuki_sound_play_background_sfx(sound_id);
}

/* Starts the remembered background sound again. */
void main_sound_replay_bg_sfx(void) {
    int sound_id = g_main_sound_weather_sfx_id;

    if (sound_id != 0) {
        suzuki_sound_play_background_sfx(sound_id);
    }
}

/* Sets the volume of the SFX channels playing sound_id: volume is the new
 * channel velocity, and 0 stops them (and forgets a remembered background
 * sound). */
void main_sound_set_sfx_echo(int sound_id, int volume) {
    if (g_main_sound_weather_sfx_id == sound_id && volume == 0) {
        g_main_sound_weather_sfx_id = 0;
    }
    suzuki_sound_set_sfx_volume(sound_id, volume);
}

/* Forgets the remembered background sound and silences every SFX channel.
 * It leaves the music alone. */
void main_sound_stop_weather_sfx_music(void) {
    g_main_sound_weather_sfx_id = 0;
    suzuki_sound_stop_all_sfx();
}

/* Stops the SFX channels playing sound_id, forgetting it if it is the
 * remembered background sound. */
void main_sound_stop_sfx(int sound_id) {
    if (sound_id == g_main_sound_weather_sfx_id) {
        g_main_sound_weather_sfx_id = 0;
    }
    suzuki_sound_stop_sfx_channels(sound_id);
}

/* Stops the remembered background sound but keeps its id, so
 * main_sound_replay_bg_sfx can restart it. */
void main_sound_stop_current_weather_sfx(void) {
    int sound_id = g_main_sound_weather_sfx_id;

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
 * five files of g_main_sound_permanent_smd_files are read into SMD slots 3..7. */
void main_sound_open_generic_sfx(void) {
    s32 i;

    suzuki_sound_init(0);
    main_heap_clear_smd_allocator_table();
    suzuki_sound_load_waveset(
        main_file_load_to_address(DISC_SECTOR_SOUND_WAVESET_WD, WAVESET_WD_SIZE, g_main_heap_low_overlay_load_address));
    suzuki_sound_append_resource(
        main_file_load_to_address(DISC_SECTOR_SOUND_SYSTEM_SED, SYSTEM_SED_SIZE, g_main_sound_system_sed_data));
    suzuki_sound_append_resource(
        main_file_load_to_address(DISC_SECTOR_SOUND_ENV_SED, ENV_SED_SIZE, g_main_sound_env_sed_data));

    g_main_sound_music.state.current_smd = 0;
    g_main_sound_music.slots.tune = 0;
    g_main_sound_music.slots.loading_slot = 0;
    for (i = 6; i >= 0; i--) {
        g_main_sound_music.slots.music[i] = 0;
    }

    g_main_sound_weather_sfx_id = 0;
    g_main_sound_music.slots.smd[2]
        = main_file_get_smd(g_main_sound_permanent_smd_files[0].sector, g_main_sound_permanent_smd_files[0].size);
    g_main_sound_music.slots.smd[3]
        = main_file_get_smd(g_main_sound_permanent_smd_files[1].sector, g_main_sound_permanent_smd_files[1].size);
    g_main_sound_music.slots.smd[4]
        = main_file_get_smd(g_main_sound_permanent_smd_files[2].sector, g_main_sound_permanent_smd_files[2].size);
    g_main_sound_music.slots.smd[5]
        = main_file_get_smd(g_main_sound_permanent_smd_files[3].sector, g_main_sound_permanent_smd_files[3].size);
    g_main_sound_music.slots.smd[6]
        = main_file_get_smd(g_main_sound_permanent_smd_files[4].sector, g_main_sound_permanent_smd_files[4].size);
}
