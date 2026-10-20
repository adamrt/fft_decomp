/* An effect callback that starts the timed map transform for the rest of the child timeline. */

#include "fft/effect.h"
#include "fft/effect_script.h"
#include "fft/effect_timeline.h"
#include "fft/weather.h"
#include "psx/types.h"

/* Callback handler: when its keyframe starts, run map weather mode 0x96 (the
 * timed particle transform) for the frames left in the child timeline. */
s32 effect_map_set_timed_transform_duration(
    s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame) {
    s16 state_index = record_index;
    effect_record_t* record = &g_effect_state_records[state_index];

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;
    case EFFECT_CALLBACK_PHASE_INIT:
        weather_transform_duration_set(BATTLE_MAP_WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM,
            g_effect_child_timeline->duration - record->timeline.frame);
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_UPDATE;
        break;
    case EFFECT_CALLBACK_PHASE_UPDATE:
        break;
    case EFFECT_CALLBACK_PHASE_DESTROY:
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_IDLE;
        break;
    }
}
