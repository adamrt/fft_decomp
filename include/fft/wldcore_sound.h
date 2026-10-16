#ifndef WLDCORE_SOUND_H
#define WLDCORE_SOUND_H

/* WLDCORE's sound: the audio queue that loads, fades and unloads music in two slots, and sound effects. */

#include "psx/types.h"

/* audio queue */
/* audio_music_slots_t.flags. */
typedef enum audio_music_flag {
    AUDIO_MUSIC_PLAYING = 0x01, /* the current slot's music is audible: a new start fades it out first */
} audio_music_flag_e;

/* Playback flags followed by the loaded music IDs for slots 1 and 2. */
typedef struct audio_music_slots {
    s32 flags; /* audio_music_flag_e */
    s32 tracks[2];
} audio_music_slots_t;

/* Value of AUDIO_COMMAND_PLAY_MUSIC and of
 * audio_queue_start_music: a music slot (1 or 2) and the
 * main_music_e id to load into it. */
#define AUDIO_MUSIC(slot, music_id) (((slot) << 8) | (music_id))

/* Commands of the audio queue (audio_queue_add), and the
 * wait states audio_queue_process moves the current command
 * through. */
typedef enum audio_command {
    AUDIO_COMMAND_NONE = 0,       /* idle: takes the next queued command */
    AUDIO_COMMAND_PLAY_MUSIC = 1, /* value: AUDIO_MUSIC(slot, music id) */
    AUDIO_COMMAND_FADE_OUT = 2,   /* value << 2: fade time */
    AUDIO_COMMAND_FADE_IN = 3,    /* the current slot, to the stored volume over value << 2 */
    AUDIO_COMMAND_UNLOAD = 4,     /* value: slot, 0 for the current one */
    AUDIO_COMMAND_WAIT_LOAD = 16,
    AUDIO_COMMAND_WAIT_FADE_OUT = 17,
    AUDIO_COMMAND_WAIT_FADE_IN = 18,
} audio_command_e;

enum { AUDIO_QUEUE_CAPACITY = 16 };

/* The audio command FIFO, filled by audio_queue_add and serviced each frame by
 * audio_queue_process, followed by the music state that consumer owns. current_command and count are
 * the busy fields audio_queue_wait_until_empty and the level steps test. */
typedef struct audio_queue {
    audio_command_e commands[AUDIO_QUEUE_CAPACITY];
    s32 values[AUDIO_QUEUE_CAPACITY];
    audio_command_e current_command; /* 0x80 */
    s32 current_value;               /* 0x84 */
    s32 count;                       /* 0x88 */
    audio_music_slots_t music;       /* 0x8c */
    s32 current_slot;                /* 0x98 */
    s32 volume;                      /* 0x9c */
} audio_queue_t;
typedef char wldcore_audio_queue_size_must_be_160[sizeof(audio_queue_t) == 160 ? 1 : -1];

extern audio_queue_t g_audio_queue;
void audio_queue_add(audio_command_e command, s32 value);
void audio_queue_start_music(s32 value);
void audio_queue_process(void);
void audio_queue_wait_until_empty(void);

/* sound effects */
/* Rows of four sound numbers in the WLDCORE sound bank (id 0xE1), indexed by a sound id from 1. Row 0 is not a
 * sound row: its four bytes are the eighth value of g_tutorial_battle_event_ids (0x1a8). */
extern u8 g_sound_novel_sound_quads[][4];
void sound_play_effect(s32 sound_id);

#endif
