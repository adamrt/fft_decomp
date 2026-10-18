#ifndef MUSIC_H
#define MUSIC_H

/* Music: MAIN's scenario music slots, playback and tunes over the Suzuki sound driver, and the track requests BATTLE,
 * WORLD and the overlays make through the main stack, plus WLDCORE's music queue. */

#include "psx/types.h"

struct suzuki_music;

/* music ids */
/* Music ids select MUSIC_NN.SMD from the 100-entry file table. Only the ids
 * the code passes by number are named, after the screen that plays them;
 * event scripts and scenario data choose the others. The music test offers
 * ids 1-96 (g_music_test_music_ids). */
typedef enum music_id {
    MUSIC_NONE = 0,       /* music_play_tracks and the WLDCORE music queue skip it */
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
    MUSIC_COUNT = 100
} music_id_e;

/* loading */
s32 music_load_into_slot(s32 music_id, s32 slot);
s32 music_load_into_free_slot(s32 music_id);
s32 music_start_load_into_slot(s32 music_id, s32 slot);
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

/* set_volume fades temporarily; set_volume_target also remembers the volume
 * that restore_volume returns to. */
s32 music_set_volume_target(s32 volume, s32 time);
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
    MUSIC_TUNE_JOB_CHANGE = 5
} music_tune_e;

/* Packed tune-slot id and pending flag. PLAYING_MASK drops the pending flag once the tune starts. */
enum { MUSIC_TUNE_ID_MASK = 0x3f, MUSIC_TUNE_PLAYING_MASK = 0x7f, MUSIC_TUNE_PENDING = 0x80 };

s32 music_play_tune(s32 tune);

/* Cancel a pending tune, or fade a playing tune to silence (time 0: immediate). */
s32 music_fade_out_tune(s32 time);
s32 music_update_tunes(void);

/* track requests */
void music_play_tracks(s32 slot_1_music_id, s32 slot_2_music_id);

/* Wait while the music loads, checking on the main stack; the check always answers done. */
void music_wait_for_load(void);

/* world-map music queue */
/* Flags of music_queue_slots_t. */
typedef enum music_queue_flag {
    MUSIC_QUEUE_PLAYING = 0x01 /* the current slot's music is audible: a new start fades it out first */
} music_queue_flag_e;

/* Playback flags followed by the loaded music IDs for slots 1 and 2. */
typedef struct music_queue_slots {
    s32 flags; /* music_queue_flag_e */
    s32 tracks[2];
} music_queue_slots_t;

/* Value of MUSIC_QUEUE_COMMAND_PLAY_TRACK and of
 * music_queue_start_track: a music slot (1 or 2) and the
 * music_id_e id to load into it. */
#define MUSIC_QUEUE_TRACK(slot, music_id) (((slot) << 8) | (music_id))

/* Commands of the music queue (music_queue_add), and the
 * wait states music_queue_process moves the current command
 * through. */
typedef enum music_queue_command {
    MUSIC_QUEUE_COMMAND_NONE = 0,       /* idle: takes the next queued command */
    MUSIC_QUEUE_COMMAND_PLAY_TRACK = 1, /* value: MUSIC_QUEUE_TRACK(slot, music id) */
    MUSIC_QUEUE_COMMAND_FADE_OUT = 2,   /* value << 2: fade time */
    MUSIC_QUEUE_COMMAND_FADE_IN = 3,    /* the current slot, to the stored volume over value << 2 */
    MUSIC_QUEUE_COMMAND_UNLOAD = 4,     /* value: slot, 0 for the current one */
    MUSIC_QUEUE_COMMAND_WAIT_LOAD = 16,
    MUSIC_QUEUE_COMMAND_WAIT_FADE_OUT = 17,
    MUSIC_QUEUE_COMMAND_WAIT_FADE_IN = 18
} music_queue_command_e;

enum { MUSIC_QUEUE_CAPACITY = 16 };

/* The music command FIFO, filled by music_queue_add and serviced each frame by
 * music_queue_process, followed by the music state that consumer owns. current_command and count are
 * the busy fields music_queue_wait_until_empty and the level steps test. */
typedef struct music_queue {
    music_queue_command_e commands[MUSIC_QUEUE_CAPACITY];
    s32 values[MUSIC_QUEUE_CAPACITY];
    music_queue_command_e current_command; /* 0x80 */
    s32 current_value;                     /* 0x84 */
    s32 count;                             /* 0x88 */
    music_queue_slots_t music;             /* 0x8c */
    s32 current_slot;                      /* 0x98 */
    s32 volume;                            /* 0x9c */
} music_queue_t;
typedef char music_queue_size_must_be_160[sizeof(music_queue_t) == 160 ? 1 : -1];

extern music_queue_t g_music_queue;

void music_queue_add(music_queue_command_e command, s32 value);
void music_queue_start_track(s32 value);
void music_queue_process(void);
void music_queue_wait_until_empty(void);

#endif
