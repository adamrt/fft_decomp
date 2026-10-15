#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"

/* Opcode 26: spawn this frame's particles of the emitter named by bits 10-13
 * of the 16-bit instruction header, at the record's frame counter. */
effect_script_result_e effect_code_script_26_spawn_emitter_particles(effect_record_t* record) {
    /* A 16-bit header cache preserves the address operands; a direct field read changes them. */
    union {
        u16 packed;
        struct {
            u16 : 10;
            u16 emitter_index : 4;
            u16 : 2;
        } fields;
    } header;

    header.packed = effect_get_current_script_header(record);

    effect_spawn_emitter_particles(record->record_index, record->frame_counter, header.fields.emitter_index, 0);
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 27: take the gravity vector and the inertia threshold from the
 * effect file's particle-system section. */
effect_script_result_e effect_code_script_27_init_particle_physics(effect_record_t* record) {
    effect_set_gravity(&g_effect_geometry_table->gravity);
    effect_set_inertia_threshold(g_effect_geometry_table->inertia_threshold);
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/*
 * Dispatch one keyframe's action flags to a single effect target: bits 4-6
 * and 8-15 trigger unit reactions (bit 3 selects the caster entry), bits 0-2
 * register a callback slot.
 */
void effect_run_keyframe_actions(u16 flags, u8 target, effect_record_t* state) {
    s32 runtime_id;
    u32 animation;
    u16 slot;

    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
        runtime_id = -1;
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            runtime_id = g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id;
        }
    } else {
        runtime_id = -1;
        if (g_effect_context.targets[target].target_type == EFFECT_TARGET_TYPE_UNIT) {
            runtime_id = g_effect_context.targets[target].data.unit.runtime_id;
        }
    }
    if ((flags & EFFECT_KEYFRAME_ACTION_FLAG_POST_ACTION_DISPLAY) && runtime_id != -1) {
        battle_gfx_prepare_post_action_display_by_runtime_id(runtime_id);
    }
    if ((flags & EFFECT_KEYFRAME_ACTION_FLAG_UPDATE_DISPLAY) && runtime_id != -1) {
        battle_unit_update_display_by_runtime_id(runtime_id);
    }
    if ((flags & EFFECT_KEYFRAME_ACTION_FLAG_TARGET_ANIMATION) && runtime_id != -1) {
        battle_unit_set_target_animation_by_runtime_id(runtime_id);
    }
    animation = (flags & EFFECT_KEYFRAME_ACTION_ANIMATION_MASK) >> EFFECT_KEYFRAME_ACTION_ANIMATION_SHIFT;
    if (animation != 0 && runtime_id != -1) {
        battle_unit_set_specific_animation_by_runtime_id(runtime_id, animation);
    }
    if (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) {
        slot = (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) - 1;
        state->callback_phase[slot] = EFFECT_CALLBACK_PHASE_INIT;
        state->callbacks[slot] = g_effect_callback_slots[slot];
    }
}

/* Returns the next sound of an effect sound channel, chosen from the channel's
 * three ids by its mode and the number of sounds it has played; 0 is no sound.
 *
 * Modes above 4 fall through with an unassigned result, as in the target. */
s32 effect_select_sound_id(s32 channel) {
    s32 count;
    s32 sound;

    count = g_effect_sound_call_counts[channel]++;
    switch (g_effect_flags_section->channels[channel].mode) {
    case EFFECT_SOUND_MODE_FIRST:
        sound = g_effect_flags_section->channels[channel].sound_ids[0];
        break;
    case EFFECT_SOUND_MODE_ALTERNATE:
        sound = g_effect_flags_section->channels[channel].sound_ids[count & 1];
        break;
    case EFFECT_SOUND_MODE_FIRST_THEN_SECOND:
        if (count == 0) {
            sound = g_effect_flags_section->channels[channel].sound_ids[0];
        } else {
            sound = g_effect_flags_section->channels[channel].sound_ids[1];
        }
        break;
    case EFFECT_SOUND_MODE_FIRST_THEN_ALTERNATE:
        if (count != 0) {
            sound = g_effect_flags_section->channels[channel].sound_ids[1 + (count & 1)];
        } else {
            sound = g_effect_flags_section->channels[channel].sound_ids[0];
        }
        break;
    case EFFECT_SOUND_MODE_CYCLE:
        sound = g_effect_flags_section->channels[channel].sound_ids[count % 3];
        break;
    }
    return sound;
}
