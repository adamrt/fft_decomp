#include "fft/battle.h"

/* Allocate one effect record and link it into the in-use list. */
s32 battle_effect_alloc_state_record(void) {
    s32 index;
    u16 next;
    /* Keep the available-list base in its target register. */
    u16* allocated;
    register s16* available __asm__("$7");
    /* Unused local: the target reserves an 8-byte frame. */
    s32 unused;

    allocated = &g_battle_effect_allocated_record_head;
    available = (s16*)&g_battle_effect_available_record_head;
    index = *available;
    next = *allocated;
    *available = g_effect_state_records[index].next_index;
    g_effect_state_records[index].next_index = next;
    *allocated = index;
    g_effect_state_records[index].flags |= 1;
    (void)&unused;
    return index;
}
