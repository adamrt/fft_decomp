#ifndef WLDCORE_SOUND_H
#define WLDCORE_SOUND_H

/* WLDCORE's sound: the audio queue that loads, fades and unloads music in two slots, and sound effects. */

#include "psx/types.h"

/* audio queue */
/* wldcore_audio_music_slots_t.flags. */
typedef enum wldcore_audio_music_flag {
    WLDCORE_AUDIO_MUSIC_PLAYING = 0x01, /* the current slot's music is audible: a new start fades it out first */
} wldcore_audio_music_flag_e;

/* Playback flags followed by the loaded music IDs for slots 1 and 2. */
typedef struct wldcore_audio_music_slots {
    s32 flags; /* wldcore_audio_music_flag_e */
    s32 tracks[2];
} wldcore_audio_music_slots_t;

/* Value of WLDCORE_AUDIO_COMMAND_PLAY_MUSIC and of
 * wldcore_sound_enqueue_music_start: a music slot (1 or 2) and the
 * main_music_e id to load into it. */
#define WLDCORE_AUDIO_MUSIC(slot, music_id) (((slot) << 8) | (music_id))

/* Commands of the audio queue (wldcore_sound_enqueue_audio_command), and the
 * wait states wldcore_sound_process_audio_queue moves the current command
 * through. */
typedef enum wldcore_audio_command {
    WLDCORE_AUDIO_COMMAND_NONE = 0,       /* idle: takes the next queued command */
    WLDCORE_AUDIO_COMMAND_PLAY_MUSIC = 1, /* value: WLDCORE_AUDIO_MUSIC(slot, music id) */
    WLDCORE_AUDIO_COMMAND_FADE_OUT = 2,   /* value << 2: fade time */
    WLDCORE_AUDIO_COMMAND_FADE_IN = 3,    /* the current slot, to the stored volume over value << 2 */
    WLDCORE_AUDIO_COMMAND_UNLOAD = 4,     /* value: slot, 0 for the current one */
    WLDCORE_AUDIO_COMMAND_WAIT_LOAD = 16,
    WLDCORE_AUDIO_COMMAND_WAIT_FADE_OUT = 17,
    WLDCORE_AUDIO_COMMAND_WAIT_FADE_IN = 18,
} wldcore_audio_command_e;

enum { WLDCORE_AUDIO_QUEUE_CAPACITY = 16 };

/* The audio command FIFO, filled by wldcore_sound_enqueue_audio_command and serviced each frame by
 * wldcore_sound_process_audio_queue, followed by the music state that consumer owns. current_command and count are
 * the busy fields wldcore_sound_wait_for_queue_drain and the level steps test. */
typedef struct wldcore_audio_queue {
    wldcore_audio_command_e commands[WLDCORE_AUDIO_QUEUE_CAPACITY];
    s32 values[WLDCORE_AUDIO_QUEUE_CAPACITY];
    wldcore_audio_command_e current_command; /* 0x80 */
    s32 current_value;                       /* 0x84 */
    s32 count;                               /* 0x88 */
    wldcore_audio_music_slots_t music;       /* 0x8c */
    s32 current_slot;                        /* 0x98 */
    s32 volume;                              /* 0x9c */
} wldcore_audio_queue_t;
typedef char wldcore_audio_queue_size_must_be_160[sizeof(wldcore_audio_queue_t) == 160 ? 1 : -1];

extern wldcore_audio_queue_t g_wldcore_audio_queue;
void wldcore_sound_enqueue_audio_command(wldcore_audio_command_e command, s32 value);
void wldcore_sound_enqueue_music_start(s32 value);
void wldcore_sound_process_audio_queue(void);
void wldcore_sound_wait_for_queue_drain(void);

/* sound effects */
/* Rows of four sound numbers in the WLDCORE sound bank (id 0xE1), indexed by a sound id from 1. Row 0 is not a
 * sound row: its four bytes are the eighth value of g_wldcore_tutorial_event_ids (0x1a8). */
extern u8 g_wldcore_sound_quad_command_bytes[][4];
void wldcore_sound_play_effect(s32 sound_id);

#endif
