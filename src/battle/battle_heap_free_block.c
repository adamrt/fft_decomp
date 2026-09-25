#include "fft/battle.h"

/*
 * Release a block from battle_heap_alloc_block.
 *
 * The block is unlinked from its owner's list, then returned to the circular
 * free list in address order and coalesced with both neighbours, as in the
 * K&R free().
 */
void battle_heap_free_block(void* ptr) {
    battle_heap_node_t* node;
    battle_heap_owner_list_t* list;
    battle_heap_node_t* owned;
    battle_heap_node_t* prev;
    battle_heap_node_t* free_node;

    node = (battle_heap_node_t*)ptr - 1;
    list = &g_battle_heap_owner_lists[node->owner];
    owned = list->head;
    if (owned == node) {
        list->head = node->next;
    } else {
        for (prev = owned; (owned = prev->next) != node; prev = owned) { }
        prev->next = node->next;
    }

    for (free_node = g_battle_heap_rover; !(node > free_node && node < free_node->next); free_node = free_node->next) {
        if (free_node >= free_node->next && (node > free_node || node < free_node->next)) {
            break;
        }
    }
    if (node + node->size == free_node->next) {
        node->size += free_node->next->size;
        node->next = free_node->next->next;
    } else {
        node->next = free_node->next;
    }
    if (free_node + free_node->size == node) {
        free_node->size += node->size;
        free_node->next = node->next;
    } else {
        free_node->next = node;
    }
    g_battle_heap_rover = free_node;
}
