/* An effect callback that starts the timed map transform for the rest of the child timeline. */

#include "fft/battle_effect.h"
#include "fft/battle_effect_script.h"
#include "fft/battle_effect_timeline.h"
#include "fft/battle_map_weather.h"
#include "fft/effect.h"
#include "psx/types.h"

/* Callback handler: when its keyframe starts, run map weather mode 0x96 (the
 * timed particle transform) for the frames left in the child timeline. */
s32 effect_map_set_timed_transform_duration(
    s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame) {
    s16 state_index = record_index;
    effect_record_t* record = &g_battle_effect_state_records[state_index];

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;
    case EFFECT_CALLBACK_PHASE_INIT:
        battle_map_set_command_0x96_duration(MAP_WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM,
            g_battle_effect_child_timeline->duration - record->timeline.frame);
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_UPDATE;
        break;
    case EFFECT_CALLBACK_PHASE_UPDATE:
        break;
    case EFFECT_CALLBACK_PHASE_DESTROY:
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_IDLE;
        break;
    }
}
