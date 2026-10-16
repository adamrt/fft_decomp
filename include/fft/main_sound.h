#ifndef MAIN_SOUND_H
#define MAIN_SOUND_H

/* Resident sound requests: music, tunes and sound effects. */

#include "fft/main.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* sound */
extern u8 g_main_sound_env_sed_data[];
extern u8 g_main_sound_system_sed_data[];
extern main_sound_music_t g_main_sound_music;
extern file_extent_t g_main_sound_scenario_smd_files[];
extern file_extent_t g_main_sound_permanent_smd_files[5];
extern s32 g_main_sound_weather_sfx_id; /* the background sound main_sound_play_bg_sfx started, 0 for none */
void main_sound_set_sfx_echo(int sound_id, int volume);
void main_sound_stop_weather_sfx_music(void);
void main_sound_stop_current_weather_sfx(void);
s32 main_sound_get_current_scenario_music(void);
suzuki_music_t* main_sound_get_music_handle(s32 slot);
/* Verified SCUS scenario-music and tune wrappers. Include this header in
 * their definitions as well as callers so ABI declarations cannot drift. */
s32 main_sound_open_and_play_music(s32 music_id, s32 slot);
void main_sound_open_generic_sfx(void);
s32 main_sound_open_music_into_free_slot(s32 music_id);
s32 main_sound_open_music_into_slot(s32 music_id, s32 slot);
s32 main_sound_open_scenario_smd_files(s32 music_id);
void main_sound_play_sfx(s32 sound_id);
void main_sound_play_bg_sfx(s32 sound_id);
void main_sound_play_sfx_find_channel(s32 sound_id);
s32 main_sound_play_tune(s32 tune);
s32 main_sound_poll_scenario_smd_load(void);
s32 main_sound_set_current_music_balance_shift_timed(s32 value, s32 time);
s32 main_sound_set_current_music_pitch_shift_timed(s32 value, s32 time);
s32 main_sound_set_current_music_target(s32 volume, s32 time);
s32 main_sound_set_current_music_tempo_scale_timed(s32 value, s32 time);
void main_sound_set_current_music_track(s32 music_id);
s32 main_sound_set_current_music_volume(s32 volume, s32 time);
s32 main_sound_set_current_music_volume_timed(s32 time);
s32 main_sound_set_tune_volume(s32 time);
s32 main_sound_stop_current_music(void);
/* battle_map_step_init_sequence calls this without loading $a0, but the body reads sound_id. */
void main_sound_stop_sfx(int sound_id);
suzuki_music_t* main_sound_switch_music_track(s32 slot, s32 volume, s32 fade);
void main_sound_unload_current_scenario_music(void);
s32 main_sound_unload_scenario_mus(s32 slot);
void main_sound_unload_scenario_music_and_tunes(void);
s32 main_sound_update_tunes(void);
s32 main_sound_wait_for_scenario_smd_load(s32 music_id);
void main_sound_replay_bg_sfx(void);
s32 main_sound_is_music_loading_stub(void);

#endif
