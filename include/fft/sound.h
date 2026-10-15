#ifndef SOUND_H
#define SOUND_H

/* Music requests, scene sound effects and weather audio. Resident music
 * loading and sound-driver adapters are declared in main.h. */

#include "psx/types.h"

struct battle_unit;

/* music */
extern s16 g_sound_music_set_track_request;
/* SwitchTrack's first operand is a trigger, not the selected track ID.
 * The consumer toggles the zero-based index, then uses main music slot 1/2. */
extern s16 g_sound_music_switch_request; /* 0x80165fc8 */
/* Track IDs are unsigned; -1 in the signed unload request means no request. */
extern u16 g_sound_music_track_1_id;
extern u16 g_sound_music_track_2_id;
extern u16 g_sound_music_track_index;         /* 0x80165fd8; zero-based music slot */
extern s16 g_sound_music_unload_slot_request; /* 0x80165fca */
/* 0x80165fc0: -1 absent; volume = low 16 bits,
 * duration = (packed >> 14) & 0x3ffc. Not a linear duration field. */
extern s32 g_sound_music_volume_transition_request;

void sound_play_music_tracks(s32 slot_1_music_id, s32 slot_2_music_id);
void sound_wait_for_music_stub(void);

/* queued effects */
extern s32 g_sound_pending_sfx_id; /* main_sfx_e; -1 means no request */
extern s32 g_sound_suppressed;

void sound_queue_sfx_cancel(void);
void sound_queue_sfx_confirm(void);
void sound_queue_sfx_confirm_unless_restoring(void);
void sound_queue_sfx_cursor_move(void);
void sound_queue_sfx_invalid(void);

/* unit effects */
extern u8 g_sound_shield_deflect_sfx_ids[];
extern u8 g_sound_weapon_hit_sfx_ids[];
/* main_sfx_e by the weapon's item_type_e (0x20 entries each): the swing, the
 * hit, and the hit a shield or Golem stops (sound_play_weapon_sfx). */
extern u8 g_sound_weapon_swing_sfx_ids[];

void sound_play_landing_sfx_by_surface_type(struct battle_unit* unit);
void sound_play_movement_sfx(struct battle_unit* unit, s32 sound_id);
void sound_play_weapon_sfx(struct battle_unit* unit, s32 hit);

/* weather */
extern s32 g_sound_primary_weather_sfx_id;
extern s32 g_sound_secondary_weather_sfx_id;
extern s32 g_sound_tertiary_weather_sfx_id;
extern u8 g_sound_weather_sfx_enabled;

void sound_start_weather_sfx(void);
s32 sound_stop_weather_sfx(void);

/* threads */
void sound_run_background_fade_thread(void);
void sound_run_background_sfx_thread(void);

/* unified */
extern u8 g_sound_queued_effect_id; /* main_sfx_e */

#endif
