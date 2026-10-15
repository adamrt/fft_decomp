#include "fft/battle.h"
#include "fft/battle_effect.h"

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
