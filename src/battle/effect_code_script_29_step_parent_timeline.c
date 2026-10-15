#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/map.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Opcode 29: advance the record's parent timeline by one frame. The second
 * 16-bit value is the script offset its child records start at.
 *
 * Before phase1_duration the phase 1 particle channels, color tracks and
 * sound tracks run (and the time scale follows the timing curve under
 * EFFECT_FLAG_TIMING_CURVE_PHASE1); afterwards one child record per effect
 * target is started, spawn_delay frames apart. Phase 2 runs from
 * phase1_duration + (target_count - 1) * spawn_delay + child_duration. The
 * camera tracks advance every frame. */
effect_script_result_e effect_code_script_29_step_parent_timeline(effect_record_t* record) {
    effect_record_timeline_t* timeline;
    s32 i;
    s16 child;
    u8 unused_stack[16]; /* Unread locals that size the target's 0x48-byte frame. */

    timeline = &record->timeline;
    if (timeline->frame >= g_effect_timeline->phase1_duration) {
        while (timeline->state.parent.spawn_countdown == 0
            && timeline->state.parent.spawned_count < g_effect_context.target_count) {
            const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

            child = effect_start_script_record(record->script, instruction->immediate.value, record->record_index);
            g_effect_state_records[child].timeline.target_index = timeline->state.parent.spawned_count;
            timeline->state.parent.spawned_count++;
            timeline->state.parent.spawn_countdown += g_effect_timeline->spawn_delay;
        }
        timeline->state.parent.spawn_countdown--;
    } else {
        for (i = 0; i < EFFECT_PARTICLE_CHANNEL_COUNT; i++) {
            effect_step_emitter_timeline(record, &g_effect_timeline->particle_channels[0][i],
                &timeline->state.parent.keyframe.tracks.particle[0][i],
                &timeline->state.parent.remaining.tracks.particle[0][i],
                &timeline->state.parent.step.tracks.particle[0][i]);
        }
        effect_advance_target_palette_track(&g_effect_timeline->color_tracks[0].surroundings,
            &timeline->state.parent.keyframe.tracks.surroundings[0],
            &timeline->state.parent.remaining.tracks.surroundings[0]);
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            effect_advance_caster_palette_track(&g_effect_timeline->color_tracks[0].caster,
                &timeline->state.parent.keyframe.tracks.caster[0], &timeline->state.parent.remaining.tracks.caster[0],
                g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id);
        }
        effect_advance_coord_target_palette_track(&g_effect_timeline->color_tracks[0].target,
            &timeline->state.parent.keyframe.tracks.target[0], &timeline->state.parent.remaining.tracks.target[0]);
        effect_advance_map_background_track(&g_effect_timeline->color_tracks[0].screen,
            &timeline->state.parent.keyframe.tracks.screen[0], &timeline->state.parent.remaining.tracks.screen[0]);
        effect_advance_parent_sound_track(&g_effect_timeline->sound_tracks[0][0],
            &timeline->state.parent.keyframe.tracks.sound[0][0], &timeline->state.parent.remaining.tracks.sound[0][0],
            &timeline->state.parent.step.tracks._unknown_24[0][0][1]);
        effect_advance_parent_sound_track(&g_effect_timeline->sound_tracks[0][1],
            &timeline->state.parent.keyframe.tracks.sound[1][0], &timeline->state.parent.remaining.tracks.sound[1][0],
            &timeline->state.parent.step.tracks._unknown_24[1][0][1]);
        effect_advance_parent_sound_track(&g_effect_timeline->sound_tracks[0][2],
            &timeline->state.parent.keyframe.tracks.sound[2][0], &timeline->state.parent.remaining.tracks.sound[2][0],
            &timeline->state.parent.step.tracks._unknown_24[2][0][1]);
        if (g_effect_flags_section->flags & EFFECT_FLAG_TIMING_CURVE_PHASE1) {
            battle_state_set_time_scale(effect_get_timing_curve_value(record));
        }
    }
    i = g_effect_timeline->phase1_duration + (g_effect_context.target_count - 1) * g_effect_timeline->spawn_delay
        + g_effect_timeline->child.duration;
    if (timeline->frame >= i) {
        for (i = 0; i < EFFECT_PARTICLE_CHANNEL_COUNT; i++) {
            effect_step_emitter_timeline(record, &g_effect_timeline->particle_channels[1][i],
                &timeline->state.parent.keyframe.tracks.particle[1][i],
                &timeline->state.parent.remaining.tracks.particle[1][i],
                &timeline->state.parent.step.tracks.particle[1][i]);
        }
        effect_advance_target_palette_track(&g_effect_timeline->color_tracks[1].surroundings,
            &timeline->state.parent.keyframe.tracks.surroundings[1],
            &timeline->state.parent.remaining.tracks.surroundings[1]);
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            effect_advance_caster_palette_track(&g_effect_timeline->color_tracks[1].caster,
                &timeline->state.parent.keyframe.tracks.caster[1], &timeline->state.parent.remaining.tracks.caster[1],
                g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id);
        }
        effect_advance_coord_target_palette_track(&g_effect_timeline->color_tracks[1].target,
            &timeline->state.parent.keyframe.tracks.target[1], &timeline->state.parent.remaining.tracks.target[1]);
        effect_advance_map_background_track(&g_effect_timeline->color_tracks[1].screen,
            &timeline->state.parent.keyframe.tracks.screen[1], &timeline->state.parent.remaining.tracks.screen[1]);
        effect_advance_parent_sound_track(&g_effect_timeline->sound_tracks[1][0],
            &timeline->state.parent.keyframe.tracks.sound[0][1], &timeline->state.parent.remaining.tracks.sound[0][1],
            &timeline->state.parent.step.tracks._unknown_24[0][1][1]);
        effect_advance_parent_sound_track(&g_effect_timeline->sound_tracks[1][1],
            &timeline->state.parent.keyframe.tracks.sound[1][1], &timeline->state.parent.remaining.tracks.sound[1][1],
            &timeline->state.parent.step.tracks._unknown_24[1][1][1]);
        effect_advance_parent_sound_track(&g_effect_timeline->sound_tracks[1][2],
            &timeline->state.parent.keyframe.tracks.sound[2][1], &timeline->state.parent.remaining.tracks.sound[2][1],
            &timeline->state.parent.step.tracks._unknown_24[2][1][1]);
    }
    camera_advance_effect_tracks(timeline->frame);
    timeline->frame++;
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
