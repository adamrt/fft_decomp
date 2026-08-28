#include "fft/effect.h"

effect_list_node_t* battle_effect_alloc_list_node(s16 kind) {
    u16* free_head = &g_battle_effect_free_list_node_head;
    u16* list_head = &g_battle_effect_active_list_node_head;
    u16 index = *free_head;
    effect_list_node_t* node = &g_battle_effect_list_nodes[index];

    *free_head = node->next;
    g_battle_effect_list_nodes[node->next].prev = 0;
    g_battle_effect_list_nodes[*list_head].prev = index;
    node->next = *list_head;
    node->prev = 0;
    *list_head = index;
    node->sprite_block = battle_effect_alloc_sprite_block(1);
    node->screen_rotation_angle = 0;
    node->frame_timer = 0;
    node->sequence_data = 0;
    node->sequence_offset = 0;
    node->frame_group_index = 0;
    node->sprite_offset_y = 0;
    node->sprite_offset_x = 0;
    node->kind = kind;
    node->sprite_frame_index = 0;
    return node;
}
