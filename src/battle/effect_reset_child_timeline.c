#include "fft/battle.h"
#include "fft/battle_effect.h"

/* Clears a record's child-timeline state and sets the effect target it plays
 * on. */
void effect_reset_child_timeline(effect_record_t* record, s16 target_index) {
    s32 index;

    for (index = 0; index < EFFECT_CHILD_TRACK_RESET_SLOT_COUNT; index++) {
        record->timeline.state.child.step.reset_slots[index] = 0;
        record->timeline.state.child.remaining.reset_slots[index] = 0;
        record->timeline.state.child.keyframe.reset_slots[index] = 0;
    }
    record->timeline.frame = 0;
    record->timeline.target_index = target_index;
}
