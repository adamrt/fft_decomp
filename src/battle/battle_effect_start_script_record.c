#include "fft/effect.h"

struct temp_effect_data_2_t;
void battle_effect_reset_parent_timeline(struct temp_effect_data_2_t* data);

/* Allocate an effect state record for a script and reset its per-handler state.
 * Returns the record index, or 0 when none is available. */
s16 battle_effect_start_script_record(u8* script, s16 start_pc, s16 parent_index) {
    s32 index;
    s16 i;
    effect_record_t* record;

    index = battle_effect_alloc_state_record();
    if ((s16)index == 0) {
        return 0;
    }
    record = &g_effect_state_records[(s16)index];
    record->parent_index = parent_index;
    record->script = script;
    record->pc = start_pc;
    for (i = 0; i < 4; i++) {
        record->counters[i] = 0;
        record->phase_ids[i] = 0;
    }
    for (i = 0; i < 4; i++) {
        record->values_d4[i] = 0;
        record->phase[i] = EFFECT_PHASE_IDLE;
        record->work_slots[i] = 0;
    }
    record->flags = 1;
    record->work_head = 0;
    record->work_count = 0;
    record->emitter_index = -1;
    battle_effect_reset_child_timeline((battle_effect_temp_data_t*)record, 0);
    battle_effect_reset_parent_timeline((struct temp_effect_data_2_t*)record);
    return index;
}
