#ifndef MAIN_SOUND_H
#define MAIN_SOUND_H

/* Resident sound requests: music, tunes and sound effects. */

#include "fft/main.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* The music state and slot table are one object: main_sound_open_generic_sfx
 * derives the slot pointers from the &state.current_smd base register, and the
 * tune/slot helpers reach across tune, handles and smd from single bases. */
typedef struct main_sound_music {
    main_sound_music_state_t state; /* 0x00 0x8004d95c */
    main_sound_music_slots_t slots; /* 0x10 0x8004d96c */
} main_sound_music_t;

/* Tunes of main_sound_play_tune: short pieces that fade the scenario music
 * out, play once and fade it back in (main_sound_update_tunes). Tune t is
 * MUSIC_(44 + t).SMD, kept loaded in music slot 2 + t. Tune 4 has no caller
 * in the code; EVENT_OPCODE_PLAY_TUNE passes its operand. */
typedef enum main_sound_tune {
    MAIN_SOUND_TUNE_NONE = 0,
    MAIN_SOUND_TUNE_LEVEL_UP = 1,
    MAIN_SOUND_TUNE_ITEM_FOUND = 2, /* an item found by moving, a poach, a trained monster */
    MAIN_SOUND_TUNE_JOB_LEVEL_UP = 3,
    MAIN_SOUND_TUNE_JOB_CHANGE = 5,
} main_sound_tune_e;

/* Packed tune-slot id and pending flag. */
enum {
    MAIN_SOUND_TUNE_ID_MASK = 0x3f,
    MAIN_SOUND_TUNE_PENDING = 0x80,
};

/* Sound resources. A sound id is SUZUKI_SOUND_ID(resource, sound): the id of a
 * loaded suzuki_sound_resource_t in the high 16 bits and the index of a
 * sound in it in the low. Each battle effect file brings a resource of its
 * own (battle_effect_load_sound_section). */
typedef enum main_sound_resource_id {
    MAIN_SOUND_RESOURCE_SYSTEM = 0,        /* SOUND/SYSTEM.SED: main_sfx_e */
    MAIN_SOUND_RESOURCE_ENV = 1,           /* SOUND/ENV.SED: main_env_sfx_e */
    MAIN_SOUND_RESOURCE_SOUND_NOVEL = 225, /* g_wldcore_sound_novel_sound_resource, 81 sounds */
} main_sound_resource_id_e;

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
