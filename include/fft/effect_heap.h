#ifndef EFFECT_HEAP_H
#define EFFECT_HEAP_H

/* The battle heap that effects allocate from: a first-fit allocator of 8-byte blocks with per-owner lists, and its
 * debug statistics. */

#include "psx/types.h"

/* allocator */
/* Battle heap block header: blocks are counted in 8-byte units and linked
 * either on the circular free list (g_effect_heap_rover) or on an owner list.
 * Owner 0 holds shared pools; owners 1 through 16 hold effect-script records. */
typedef struct effect_heap_node {
    struct effect_heap_node* next;
    u16 size; /* in 8-byte blocks */
    s16 owner;
} effect_heap_node_t;

void effect_heap_init(effect_heap_node_t* buffer, u32 buffer_size);
void* effect_heap_alloc_block(u32 size, s32 owner);
void effect_heap_free_block(void* ptr);

/* statistics */
void effect_heap_print_stats(void);

#endif
