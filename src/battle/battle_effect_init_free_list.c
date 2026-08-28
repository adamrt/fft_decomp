#include "fft/effect.h"

/*
 * Rebuilds the effect-node free list: node 0 is the list sentinel, nodes
 * 1..256 are chained through next with 256 terminating, and g_battle_effect_free_list_node_head holds
 * the free-list head.  The GetClut result is computed and discarded in the
 * target.
 */
void battle_effect_init_free_list(void) {
    s32 i;

    GetClut(0, 0x1f0);

    g_battle_effect_list_nodes[0].next = 0;
    g_battle_effect_list_nodes[0].prev = 0;
    g_battle_effect_free_list_node_head = 1;
    for (i = 1; i < 256; i++) {
        g_battle_effect_list_nodes[i].index = i;
        g_battle_effect_list_nodes[i].prev = i - 1;
        g_battle_effect_list_nodes[i].next = i + 1;
    }
    g_battle_effect_list_nodes[i].prev = i - 1;
    g_battle_effect_list_nodes[i].next = 0;
    g_battle_effect_list_nodes[i].index = i;
    g_battle_effect_active_list_node_head = 0;
}
