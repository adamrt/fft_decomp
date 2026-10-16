#ifndef BATTLE_SOUND_H
#define BATTLE_SOUND_H

/* BATTLE's sound: the music requests of its event scripts, unit sound effects and weather sounds. */

#include "fft/battle_unit.h"
#include "psx/types.h"

/* music requests */
/* Left by the event script's music instructions for script_process_pending_requests, which
 * carries them out once per event frame; -1 means no request. The switch request is a trigger,
 * not a track: it toggles g_battle_sound_music_track_index and starts the track in that slot
 * (g_sound_music_track_1_id or _2_id) with the volume and fade of
 * g_battle_event_music_switch_volume and g_battle_event_music_switch_fade (battle_event.h). */
extern s16 g_battle_sound_music_set_track_request;
extern s16 g_battle_sound_music_switch_request;
extern s16 g_battle_sound_music_unload_slot_request;
/* The target volume in the low 16 bits; the fade time is (packed >> 14) & 0x3ffc, not a linear
 * field. */
extern s32 g_battle_sound_music_volume_transition_request;
extern u16 g_battle_sound_music_track_index; /* zero-based music slot */

/* unit sound effects */
/* main_sfx_e by the weapon's item_type_e (0x20 entries each): the swing, the
 * hit, and the hit a shield or Golem stops (battle_sound_play_weapon_sfx). */
extern u8 g_battle_sound_weapon_swing_sfx_ids[];
extern u8 g_battle_sound_weapon_hit_sfx_ids[];
extern u8 g_battle_sound_shield_deflect_sfx_ids[];

void battle_sound_play_movement_sfx(struct battle_unit* unit, s32 sound_id);
void battle_sound_play_weapon_sfx(struct battle_unit* unit, s32 hit);

/* weather */
/* The rain sound (primary) and the two thunder sounds, 0 when none plays. */
extern s32 g_battle_sound_primary_weather_sfx_id;
extern s32 g_battle_sound_secondary_weather_sfx_id;
extern s32 g_battle_sound_tertiary_weather_sfx_id;
extern u8 g_battle_sound_weather_sfx_enabled;

void battle_sound_start_weather_sfx(void);
s32 battle_sound_stop_weather_sfx(void); /* the result is indeterminate; no caller reads it */

#endif
