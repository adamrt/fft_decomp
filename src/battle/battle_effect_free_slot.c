#include "fft/battle.h"

/* Unlinks slot id from the active chain and pushes it onto the free chain;
 * slot 0 is the null sentinel and is ignored. */
void battle_effect_free_slot(u8 id) {
    u8 free_head;

    if (id == 0) {
        return;
    }
    g_battle_effect_slots[g_battle_effect_slot_free_head].prev = id;
    if (g_battle_effect_slots[id].prev != 0) {
        g_battle_effect_slots[g_battle_effect_slots[id].prev].next = g_battle_effect_slots[id].next;
    } else {
        g_battle_effect_active_slot_head = g_battle_effect_slots[id].next;
    }
    g_battle_effect_slots[g_battle_effect_slots[id].next].prev = g_battle_effect_slots[id].prev;
    free_head = g_battle_effect_slot_free_head;
    g_battle_effect_slots[id].prev = 0;
    g_battle_effect_slots[id].next = free_head;
    g_battle_effect_slot_free_head = id;
}
