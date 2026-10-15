#include "fft/battle.h"
#include "fft/battle_effect.h"

/* Clear all 26 16-bit values in each bank, including the extra reset slots. */
void effect_reset_parent_timeline(effect_record_t* record) {
    s32 index;

    for (index = 0; index < EFFECT_PARENT_TRACK_RESET_SLOT_COUNT; index++) {
        record->timeline.state.parent.step.reset_slots[index] = 0;
        record->timeline.state.parent.remaining.reset_slots[index] = 0;
        record->timeline.state.parent.keyframe.reset_slots[index] = 0;
    }
    record->timeline.state.parent.spawn_countdown = 0;
    record->timeline.frame = 0;
    record->timeline.state.parent.spawned_count = 0;
}
