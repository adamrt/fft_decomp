#ifndef EFFECT_HEAP_H
#define EFFECT_HEAP_H

/* Declarations. */

#include "psx/libgpu.h"
#include "psx/types.h"

/* Battle heap block header: blocks are counted in 8-byte units and linked
 * either on the circular free list (g_effect_heap_rover) or on one of the 16
 * per-owner lists in g_effect_heap_owner_lists. */
typedef struct effect_heap_node {
    struct effect_heap_node* next;
    u16 size; /* in 8-byte blocks */
    s16 owner;
} effect_heap_node_t;

typedef struct effect_heap_owner_list {
    effect_heap_node_t* head;
    s32 _unused_04;
} effect_heap_owner_list_t;

extern effect_heap_node_t* g_effect_heap_base;
extern s16 g_effect_heap_block_count;
extern s32 g_effect_heap_min_largest_free; /* Lowest largest-free_node-block size seen, in 8-byte units. */
extern effect_heap_owner_list_t g_effect_heap_owner_lists[];
extern effect_heap_node_t* g_effect_heap_rover;

/* The formats effect_heap_print_stats prints. */
extern const char g_effect_heap_dp_format[]; /* "DP %x\n" */

extern const char g_effect_heap_remain_format[]; /* "HEAP REMAIN %x MAX %x\n" */
extern const char g_effect_heap_task_format[];   /* "TASK %d %x\n" */
extern const char g_effect_heap_min_format[];    /* "MIN HEAP %x " */
void* effect_heap_alloc_block(u32 size, s32 kind);
void effect_heap_free_block(void* ptr);
void effect_heap_init(effect_heap_node_t* buffer, u32 buffer_size);
void effect_heap_print_stats(void);

/* Provisional: never written; effect_heap_print_stats prints it x4 in its "DP" line. */
extern s32 g_effect_heap_debug_dp_words;

/* Provisional: never written; added to the "DP" figure of effect_heap_print_stats. */
extern s32 g_effect_heap_debug_dp_bytes;

#endif
