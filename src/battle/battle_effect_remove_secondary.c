/* Unlink secondary-effect slot `id` (1..16) from the active list, free its
 * allocation, and append it to the tail of the free list headed by
 * `g_battle_effect_free_slot_head`. This is the inverse of
 * `battle_effect_allocate_secondary_slot`; out-of-range IDs report error 0x67.
 * The free-list walk index is an s32: a u16 adds an `andi 0xffff` per load.
 */
#include "fft/battle.h"

void battle_effect_remove_secondary(u8 id) {
    battle_effect_secondary_data_t* slot;
    s32 tail;

    slot = &g_battle_effect_secondary_data[id];
    if (id != 0 && id < 17) {
        if (slot->allocation != 0) {
            main_heap_free(slot->allocation);
            slot->allocation = 0;
        }
        if (slot->next_slot_id != 0) {
            g_battle_effect_secondary_data[slot->next_slot_id].previous_slot_id = slot->previous_slot_id;
        } else {
            g_battle_effect_active_slot_tail = slot->previous_slot_id;
        }
        g_battle_effect_secondary_data[g_battle_effect_active_slot_tail].next_slot_id = 0;
        tail = g_battle_effect_free_slot_head;
        if (tail != 0) {
            while (g_battle_effect_secondary_data[tail].previous_slot_id != 0) {
                tail = g_battle_effect_secondary_data[tail].previous_slot_id;
            }
            g_battle_effect_secondary_data[tail].previous_slot_id = id;
            slot->next_slot_id = tail;
        } else {
            g_battle_effect_free_slot_head = id;
            slot->next_slot_id = 0;
        }
        slot->previous_slot_id = 0;
        slot->phase = 0;
        return;
    }
    main_noop_800449f8(1, 0x67);
}
