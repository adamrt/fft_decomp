#include "fft/battle.h"

/*
 * Debug-print battle heap usage: total and largest free_node block on the free_node
 * ring, the low-water mark of the largest block, a "DP" value, and the bytes
 * held by each of the 16 allocation owners.
 *
 * The ring-end test rereads g_battle_heap_rover; GCC hoists that load, which
 * produces the target copy of the rover into a separate register.
 */
void battle_heap_print_stats(void) {
    battle_heap_node_t* free_node;
    battle_heap_node_t* node;
    s32 total;
    s32 largest;
    s32 owner;
    s32 used;
    s32 size;

    total = 0;
    largest = 0;
    free_node = g_battle_heap_rover->next;
    for (;;) {
        size = free_node->size;
        total += size;
        if (largest < size) {
            largest = size;
        }
        if (free_node == g_battle_heap_rover) {
            break;
        }
        free_node = free_node->next;
    }
    FntPrint(g_battle_text_heap_remain_format, total * 8, largest * 8);
    if (largest < g_battle_heap_min_largest_free) {
        g_battle_heap_min_largest_free = largest;
    }
    FntPrint(g_battle_text_min_heap_format, g_battle_heap_min_largest_free * 8);
    FntPrint(g_battle_text_heap_dp_format, D_801BACC4 * 4 + D_801BC0D8);
    for (owner = 0; owner < 16; owner++) {
        used = 0;
        for (node = g_battle_heap_owner_lists[owner].head; node != 0; node = node->next) {
            used += node->size;
        }
        if (used != 0) {
            FntPrint(g_battle_text_heap_task_format, owner, used * 8);
        }
    }
}
