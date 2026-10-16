#ifndef SOUND_H
#define SOUND_H

/* Event library sound for BATTLE and WORLD: the scenario music tracks, the queued menu sound effects and the
 * background sound threads. Resident music loading and the sound-driver adapters are declared in main_sound.h. */

#include "psx/types.h"

/* music */
/* The scenario's music tracks in main music slots 1 and 2; 0 is none. */
extern u16 g_sound_music_track_1_id;
extern u16 g_sound_music_track_2_id;

void sound_play_music_tracks(s32 slot_1_music_id, s32 slot_2_music_id);
/* Wait while the music loads, checking on the main stack; the check always answers done. */
void sound_wait_for_music_load(void);

/* queued effects */
extern s32 g_sound_pending_sfx_id; /* main_sfx_e; -1 means no request */
extern s32 g_sound_suppressed;
extern u8 g_sound_queued_effect_id; /* main_sfx_e */

void sound_queue_sfx_cancel(void);
void sound_queue_sfx_confirm(void);
void sound_queue_sfx_cursor_move(void);
void sound_queue_sfx_invalid(void);

/* background sound threads */
void sound_run_background_fade_thread(void);
void sound_run_background_sfx_thread(void);

#endif
