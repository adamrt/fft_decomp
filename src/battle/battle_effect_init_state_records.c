#include "fft/battle.h"

/* Initialize the effect record pool and its intrusive free list.
 *
 * Record 0 is the empty-list sentinel; records 1 through 16 begin available. */
void battle_effect_init_state_records(void) {
    s32 i;

    g_effect_state_records[0].next_index = 0;
    g_battle_effect_available_record_head = 1;
    for (i = 1; i < 16; i++) {
        g_effect_state_records[i].record_index = i;
        g_effect_state_records[i].next_index = i + 1;
        g_effect_state_records[i].work_slots[4] = 0;
    }
    g_effect_state_records[i].next_index = 0;
    g_effect_state_records[i].record_index = i;
    g_battle_effect_allocated_record_head = 0;
    g_battle_effect_current_record_index = 0;
    g_battle_effect_phase = 0;
}
