/* Effect record pool with its free list, timeline resets and the script record start. */

#include "fft/battle.h"
#include "fft/battle_effect.h"

/* Initialize the effect record pool and its intrusive free list.
 *
 * Record 0 is the empty-list sentinel; records 1 through 16 begin available. */
void effect_init_state_records(void) {
    s32 i;

    g_effect_state_records[0].next_index = 0;
    g_effect_available_record_head = 1;
    for (i = 1; i < EFFECT_STATE_RECORD_CAPACITY; i++) {
        g_effect_state_records[i].record_index = i;
        g_effect_state_records[i].next_index = i + 1;
        g_effect_state_records[i].last_native_callback = 0;
    }
    g_effect_state_records[i].next_index = 0;
    g_effect_state_records[i].record_index = i;
    g_effect_allocated_record_head = 0;
    g_effect_current_record_index = 0;
    g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_IDLE;
}

/* Allocate one effect record and link it into the in-use list. */
s16 effect_alloc_state_record(void) {
    u16* available;
    s16* allocated;
    s16 index;
    u16 next_available;
    u16 next_allocated;

    available = &g_effect_available_record_head;
    index = *available;
    next_available = g_effect_state_records[index].next_index;
    allocated = &g_effect_allocated_record_head;
    next_allocated = *allocated;
    *available = next_available;
    g_effect_state_records[index].next_index = next_allocated;
    *allocated = index;
    g_effect_state_records[index].flags |= EFFECT_RECORD_FLAG_ALLOCATED;
    return index;
}

/* Tear down effect state record record_index and return it to the free list.
 *
 * Frees every work record on its list, releases the four handler work slots,
 * clears the spawning record's reference to it, then unlinks it from the
 * allocated list; the inverse of effect_alloc_state_record. The
 * separate `particle` copy keeps the call's work argument loaded at the loop
 * edges, as in the target, and the list-head pointers keep the target's
 * materialized global address. */
void effect_free_state_record(s16 record_index) {
    effect_work_record_t* work;
    effect_work_record_t* particle;
    s16 head;
    s16 i;
    s16 parent;
    s16 cur;

    particle = work = g_effect_state_records[record_index].work_head;
    head = g_effect_allocated_record_head;
    while (work != 0) {
        effect_free_work_record(record_index, particle);
        particle = work = work->next;
    }
    for (i = 0; i < EFFECT_RECORD_CALLBACK_SLOT_COUNT; i++) {
        if (g_effect_state_records[record_index].work_slots[i] != 0) {
            battle_heap_free_block(g_effect_state_records[record_index].work_slots[i]);
            g_effect_state_records[record_index].work_slots[i] = 0;
        }
        g_effect_state_records[record_index].callbacks[i] = 0;
    }
    parent = g_effect_state_records[record_index].parent_index;
    if (parent != 0 && (g_effect_state_records[parent].flags & EFFECT_RECORD_FLAG_ALLOCATED)) {
        for (i = 0; i < EFFECT_SCRIPT_CHILD_SLOT_COUNT; i++) {
            if (g_effect_state_records[parent].child_indices[i] == record_index) {
                g_effect_state_records[parent].child_indices[i] = 0;
            }
        }
    }
    if (head == record_index) {
        u16* available;

        g_effect_allocated_record_head = g_effect_state_records[record_index].next_index;
        available = &g_effect_available_record_head;
        g_effect_state_records[record_index].next_index = *available;
        *available = head;
    } else {
        u16* available_after_unlink;

        while ((cur = g_effect_state_records[head].next_index) != record_index) {
            head = cur;
        }
        g_effect_state_records[head].next_index = g_effect_state_records[cur].next_index;
        available_after_unlink = &g_effect_available_record_head;
        g_effect_state_records[cur].next_index = *available_after_unlink;
        *available_after_unlink = cur;
    }
    g_effect_state_records[record_index].flags &= ~EFFECT_RECORD_FLAG_ALLOCATED;
}

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

/* Allocate an effect state record for a script and reset its per-handler state.
 * Returns the record index, or 0 when none is available. */
s16 effect_start_script_record(const u8* script, s16 start_pc, s16 parent_index) {
    s16 index;
    s16 i;
    effect_record_t* record;

    index = effect_alloc_state_record();
    if (index == 0) {
        return 0;
    }
    record = &g_effect_state_records[index];
    record->parent_index = parent_index;
    record->script = script;
    record->pc = start_pc;
    for (i = 0; i < EFFECT_SCRIPT_COUNTER_COUNT; i++) {
        record->counters[i] = 0;
        record->child_indices[i] = 0;
    }
    for (i = 0; i < EFFECT_RECORD_CALLBACK_SLOT_COUNT; i++) {
        record->callbacks[i] = 0;
        record->callback_phase[i] = EFFECT_CALLBACK_PHASE_IDLE;
        record->work_slots[i] = 0;
    }
    record->flags = EFFECT_RECORD_FLAG_ALLOCATED;
    record->work_head = 0;
    record->work_count = 0;
    record->frame_counter = -1;
    effect_reset_child_timeline(record, 0);
    effect_reset_parent_timeline(record);
    return index;
}
