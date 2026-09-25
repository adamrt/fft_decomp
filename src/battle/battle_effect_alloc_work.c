#include "fft/battle.h"

/*
 * Take a work record from the free chain and attach it to state record
 * record_index; the inverse of battle_effect_free_particle.
 *
 * Bits 12-15 of kind select the effect list node type allocated as the
 * record's data (0x0000-0x3000 map to list kinds 0, 2, 4 and 6); other kinds
 * get no data. g_battle_effect_work_record_peak tracks the peak number of records in use.
 */
effect_work_record_t* battle_effect_alloc_work(s16 record_index, s32 kind) {
    effect_work_record_t* work;
    effect_record_t* owner;

    work = g_battle_effect_free_work_record_head;
    g_battle_effect_work_record_count++;
    owner = &g_effect_state_records[record_index];
    if (g_battle_effect_work_record_peak < g_battle_effect_work_record_count) {
        g_battle_effect_work_record_peak = g_battle_effect_work_record_count;
    }
    if (work == 0) {
        main_noop_800449f8(1, 0x3E);
    }
    g_battle_effect_free_work_record_head = work->next;
    switch (kind & EFFECT_WORK_KIND_MASK) {
    case EFFECT_WORK_KIND_LIST_NODE_0:
        work->data = battle_effect_alloc_list_node(0);
        break;
    case EFFECT_WORK_KIND_LIST_NODE_2:
        work->data = battle_effect_alloc_list_node(2);
        break;
    case EFFECT_WORK_KIND_LIST_NODE_4:
        work->data = battle_effect_alloc_list_node(4);
        break;
    case EFFECT_WORK_KIND_LIST_NODE_6:
        work->data = battle_effect_alloc_list_node(6);
        break;
    }
    work->prev = 0;
    work->next = owner->work_head;
    if (owner->work_head != 0) {
        owner->work_head->prev = work;
    }
    owner->work_head = work;
    owner->work_count++;
    return work;
}
