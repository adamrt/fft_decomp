#include "fft/battle_effect.h"
#include "fft/battle_state.h"
#include "fft/effect.h"
#include "psx/types.h"

/* Timeline state overlaid on effect_record_t from 0x26. */
typedef struct battle_effect_timeline_state {
    u16 target_index;             /* 0x00 (record 0x26) */
    s16 frame;                    /* 0x02 (record 0x28) */
    s16 spawn_countdown;          /* 0x04 (record 0x2a) */
    u16 spawned_count;            /* 0x06 (record 0x2c) */
    s16 particle_keyframe[2][5];  /* 0x08 */
    s16 track_keyframe[7][2];     /* 0x1c; four colour tracks then three sound tracks, per phase */
    u8 _unknown_38[4];            /* 0x38 */
    s16 particle_remaining[2][5]; /* 0x3c */
    s16 track_remaining[7][2];    /* 0x50 */
    u8 _unknown_6c[4];            /* 0x6c */
    s16 particle_step[2][5];      /* 0x70 */
    u8 _unknown_84[0x10];         /* 0x84 */
    u8 sound_state[3][2][2];      /* 0x94 */
} battle_effect_timeline_state_t;

/* The target passes a fourth argument that the sound-timer definition does not declare. */
typedef void (*battle_effect_sound_timer_fn_t)(u8* schedule, s16* entry_index, s16* countdown, u8* state);

/* Advance an effect's timeline by one frame.
 *
 * Before phase1_duration the phase 1 particle channels, colour tracks and
 * sound tracks run (and the time scale follows the timing curve when effect
 * flag 0x20 is set); afterwards one child record per effect target is spawned
 * every spawn_delay frames. Phase 2 runs from phase1_duration +
 * (target_count - 1) * spawn_delay + phase2_delay. The camera tracks advance
 * every frame. */
s32 battle_effect_code_script_29_step_parent_timeline(effect_record_t* record) {
    battle_effect_timeline_state_t* state;
    s32 i;
    s16 child;
    volatile u8 unused_stack[16]; /* Unread locals that size the target's 0x48-byte frame. */

    state = (battle_effect_timeline_state_t*)&record->target_index;
    if (state->frame >= g_battle_effect_misc_data->phase1_duration) {
        while (state->spawn_countdown == 0 && state->spawned_count < g_battle_effect_coord_data.hit_counter) {
            child = battle_effect_start_script_record(
                record->script, *(s16*)(record->pc + (s32)record->script + 2), record->record_index);
            g_effect_state_records_view[child].target_index = state->spawned_count;
            state->spawned_count++;
            state->spawn_countdown += g_battle_effect_misc_data->spawn_delay;
        }
        state->spawn_countdown--;
    } else {
        for (i = 0; i < 5; i++) {
            battle_effect_step_emitter_timeline((battle_keyframe_effect_state_t*)record,
                &g_battle_effect_misc_data->particle_channels[0][i], &state->particle_keyframe[0][i],
                &state->particle_remaining[0][i], &state->particle_step[0][i]);
        }
        battle_effect_advance_target_palette_track(&g_battle_effect_misc_data->color_tracks[0].affected_units,
            &state->track_keyframe[0][0], &state->track_remaining[0][0]);
        if (g_battle_effect_targets[16].target_type == 0) {
            battle_effect_advance_caster_palette_track(&g_battle_effect_misc_data->color_tracks[0].caster,
                &state->track_keyframe[1][0], &state->track_remaining[1][0], g_battle_effect_targets[16].id.misc_id);
        }
        battle_effect_advance_coord_target_palette_track(&g_battle_effect_misc_data->color_tracks[0].target,
            &state->track_keyframe[2][0], &state->track_remaining[2][0]);
        battle_effect_advance_map_background_track(&g_battle_effect_misc_data->color_tracks[0].screen,
            &state->track_keyframe[3][0], &state->track_remaining[3][0]);
        ((battle_effect_sound_timer_fn_t)battle_effect_update_on_hit_sound_timer)(
            g_battle_effect_misc_data->sound_tracks[0][0], &state->track_keyframe[4][0], &state->track_remaining[4][0],
            &state->sound_state[0][0][1]);
        ((battle_effect_sound_timer_fn_t)battle_effect_update_on_hit_sound_timer)(
            g_battle_effect_misc_data->sound_tracks[0][1], &state->track_keyframe[5][0], &state->track_remaining[5][0],
            &state->sound_state[1][0][1]);
        ((battle_effect_sound_timer_fn_t)battle_effect_update_on_hit_sound_timer)(
            g_battle_effect_misc_data->sound_tracks[0][2], &state->track_keyframe[6][0], &state->track_remaining[6][0],
            &state->sound_state[2][0][1]);
        if (g_battle_effect_flags_section->flags & EFFECT_FLAG_TIMING_CURVE_PHASE1) {
            battle_state_set_time_scale(battle_effect_get_timing_curve_value((battle_effect_timing_entry_t*)record));
        }
    }
    i = g_battle_effect_misc_data->phase1_duration
        + (g_battle_effect_coord_data.hit_counter - 1) * g_battle_effect_misc_data->spawn_delay
        + g_battle_effect_misc_data->phase2_delay;
    if (state->frame >= i) {
        for (i = 0; i < 5; i++) {
            battle_effect_step_emitter_timeline((battle_keyframe_effect_state_t*)record,
                &g_battle_effect_misc_data->particle_channels[1][i], &state->particle_keyframe[1][i],
                &state->particle_remaining[1][i], &state->particle_step[1][i]);
        }
        battle_effect_advance_target_palette_track(&g_battle_effect_misc_data->color_tracks[1].affected_units,
            &state->track_keyframe[0][1], &state->track_remaining[0][1]);
        if (g_battle_effect_targets[16].target_type == 0) {
            battle_effect_advance_caster_palette_track(&g_battle_effect_misc_data->color_tracks[1].caster,
                &state->track_keyframe[1][1], &state->track_remaining[1][1], g_battle_effect_targets[16].id.misc_id);
        }
        battle_effect_advance_coord_target_palette_track(&g_battle_effect_misc_data->color_tracks[1].target,
            &state->track_keyframe[2][1], &state->track_remaining[2][1]);
        battle_effect_advance_map_background_track(&g_battle_effect_misc_data->color_tracks[1].screen,
            &state->track_keyframe[3][1], &state->track_remaining[3][1]);
        ((battle_effect_sound_timer_fn_t)battle_effect_update_on_hit_sound_timer)(
            g_battle_effect_misc_data->sound_tracks[1][0], &state->track_keyframe[4][1], &state->track_remaining[4][1],
            &state->sound_state[0][1][1]);
        ((battle_effect_sound_timer_fn_t)battle_effect_update_on_hit_sound_timer)(
            g_battle_effect_misc_data->sound_tracks[1][1], &state->track_keyframe[5][1], &state->track_remaining[5][1],
            &state->sound_state[1][1][1]);
        ((battle_effect_sound_timer_fn_t)battle_effect_update_on_hit_sound_timer)(
            g_battle_effect_misc_data->sound_tracks[1][2], &state->track_keyframe[6][1], &state->track_remaining[6][1],
            &state->sound_state[2][1][1]);
    }
    battle_camera_advance_effect_tracks(state->frame);
    state->frame++;
    record->pc += 4;
    return 1;
}
