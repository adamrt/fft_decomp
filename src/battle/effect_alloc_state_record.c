#include "fft/battle.h"
#include "fft/battle_effect.h"

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
