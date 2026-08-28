#include "fft/effect.h"

s32 battle_effect_alloc_slot(void) {
    u8 head;
    u8 tail;
    s32 id;

    head = g_battle_effect_slot_free_head;
    id = head;
    if (id != 0) {
        g_battle_effect_slot_free_head = g_battle_effect_slots[id].next;
        g_battle_effect_slots[g_battle_effect_slots[id].next].prev = 0;
        g_battle_effect_slots[g_battle_effect_active_slot_head].prev = head;
        tail = g_battle_effect_active_slot_head;
        g_battle_effect_slots[id].prev = 0;
        g_battle_effect_slots[id].next = tail;
        g_battle_effect_active_slot_head = head;
    }
    g_battle_effect_slots[id].life = 0;
    return id;
}
