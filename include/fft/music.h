#ifndef MUSIC_H
#define MUSIC_H

/* Music: MAIN's scenario music slots, playback and tunes over the Suzuki sound driver, and the track requests BATTLE,
 * WORLD and the overlays make through the main stack. */

#include "psx/types.h"

struct suzuki_music;

/* music ids */
/* Music ids select MUSIC_NN.SMD from the 100-entry file table. Only the ids
 * the code passes by number are named, after the screen that plays them;
 * event scripts and scenario data choose the others. The music test offers
 * ids 1-96 (g_music_test_music_ids). */
typedef enum music_id {
    MUSIC_NONE = 0,       /* music_play_tracks and the WLDCORE audio queue skip it */
    MUSIC_ALTIMA = 19,    /* EVENT_CALL_FUNCTION_PLAY_ALTIMA_MUSIC */
    MUSIC_MILESTONE = 20, /* proposition_step_discovery_level, in the bar */
    MUSIC_TUTORIAL = 26,  /* the world menu's tutorial screen */
    MUSIC_WORLD_MAP = 27,
    MUSIC_SHOP = 28,                         /* shop_run_screen: the item shop */
    MUSIC_SOLDIER_OFFICE = 29,               /* shop_run_screen */
    MUSIC_FUR_SHOP = 30,                     /* shop_run_screen */
    MUSIC_FORMATION = 31,                    /* formation_run_screen */
    MUSIC_BRAVE_STORY = 32,                  /* the Brave Story screens */
    MUSIC_BAR = 33,                          /* the bar of a town */
    MUSIC_NEW_GAME = 34,                     /* name and birthday entry of a new game */
    MUSIC_PROPOSITION_REPORT = 35,           /* a finished proposition's report */
    MUSIC_SAVE = 41,                         /* the memory card screens */
    MUSIC_DEPLOYMENT = 42,                   /* the ATTACK unit placement screen */
    MUSIC_CONGRATULATIONS = 43,              /* victory_congratulations_thread */
    MUSIC_CONGRATULATIONS_FINAL_BATTLE = 44, /* the same screen after event 0x145 */
    MUSIC_GAME_OVER = 50,                    /* game_over_show */
    MUSIC_COUNT = 100,
} music_id_e;

/* loading */
s32 music_load_into_slot(s32 music_id, s32 slot);
s32 music_open_into_slot(s32 music_id, s32 slot);
s32 music_open_into_free_slot(s32 music_id);
s32 music_start_load_into_free_slot(s32 music_id);
s32 music_poll_scenario_load(void);
s32 music_wait_for_scenario_load(s32 music_id);
s32 music_is_loading_stub(void);
void music_unload_current_scenario(void);
s32 music_unload_scenario_slot(s32 slot);
void music_unload_scenario_and_tunes(void);

/* playback */
struct suzuki_music* music_get_handle(s32 slot);
struct suzuki_music* music_switch_track(s32 slot, s32 volume, s32 fade);
s32 music_get_current_slot(void);
void music_set_track(s32 music_id);
s32 music_set_target(s32 volume, s32 time);
s32 music_set_volume(s32 volume, s32 time);
s32 music_restore_volume(s32 time);
s32 music_set_balance_shift_timed(s32 value, s32 time);
s32 music_set_pitch_shift_timed(s32 value, s32 time);
s32 music_set_tempo_scale_timed(s32 value, s32 time);
s32 music_stop(void);

/* tunes */
/* Tunes of music_play_tune: short pieces that fade the scenario music
 * out, play once and fade it back in (music_update_tunes). Tune t is
 * MUSIC_(44 + t).SMD, kept loaded in music slot 2 + t. Tune 4 has no caller
 * in the code; EVENT_OPCODE_PLAY_TUNE passes its operand. */
typedef enum music_tune {
    MUSIC_TUNE_NONE = 0,
    MUSIC_TUNE_LEVEL_UP = 1,
    MUSIC_TUNE_ITEM_FOUND = 2, /* an item found by moving, a poach, a trained monster */
    MUSIC_TUNE_JOB_LEVEL_UP = 3,
    MUSIC_TUNE_JOB_CHANGE = 5,
} music_tune_e;

/* Packed tune-slot id and pending flag. PLAYING_MASK drops the pending flag once the tune starts. */
enum {
    MUSIC_TUNE_ID_MASK = 0x3f,
    MUSIC_TUNE_PLAYING_MASK = 0x7f,
    MUSIC_TUNE_PENDING = 0x80,
};

s32 music_play_tune(s32 tune);
s32 music_set_tune_volume(s32 time);
s32 music_update_tunes(void);

/* track requests */
void music_play_tracks(s32 slot_1_music_id, s32 slot_2_music_id);

/* Wait while the music loads, checking on the main stack; the check always answers done. */
void music_wait_for_load(void);

#endif
