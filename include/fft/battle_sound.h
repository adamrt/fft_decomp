#ifndef BATTLE_SOUND_H
#define BATTLE_SOUND_H

/* BATTLE's sound: the music requests of its event scripts, unit sound effects and weather sounds. */

#include "fft/battle_unit.h"
#include "psx/types.h"

/* music requests */
/* Left by the event script's music instructions for script_process_pending_requests, which
 * carries them out once per event frame; -1 means no request. The switch request is a trigger,
 * not a track: it toggles g_sound_current_music_track_index and starts the track in that slot
 * (g_sound_music_track_1_id or _2_id) with the volume and fade of
 * g_event_music_switch_volume and g_event_music_switch_fade (battle_event.h). */
extern s16 g_sound_music_set_track_request;
extern s16 g_sound_music_switch_request;
extern s16 g_sound_music_unload_slot_request;
/* The target volume in the low 16 bits; the fade time is (packed >> 14) & 0x3ffc, not a linear
 * field. */
extern s32 g_sound_music_volume_transition_request;
extern u16 g_sound_current_music_track_index; /* zero-based music slot */

/* unit sound effects */
/* sound_sfx_e by the weapon's item_type_e (0x20 entries each): the swing, the
 * hit, and the hit a shield or Golem stops (action_play_weapon_sfx). */
extern u8 g_action_weapon_swing_sfx_ids[];
extern u8 g_action_weapon_hit_sfx_ids[];
extern u8 g_action_shield_deflect_sfx_ids[];

void unit_play_sfx(struct unit* unit, s32 sound_id);
void action_play_weapon_sfx(struct unit* unit, s32 hit);

/* weather */
/* The rain sound (primary) and the two thunder sounds, 0 when none plays. */
extern s32 g_weather_primary_sfx_id;
extern s32 g_weather_secondary_sfx_id;
extern s32 g_weather_tertiary_sfx_id;
extern u8 g_weather_sfx_enabled;

void weather_start_sfx(void);
s32 weather_stop_sfx(void); /* the result is indeterminate; no caller reads it */

#endif
