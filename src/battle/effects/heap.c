/* Effect heap allocation, owner lists and usage diagnostics. */

#include "fft/effect_heap.h"
#include "psx/libgpu.h"
#include "psx/types.h"

enum { EFFECT_HEAP_OWNER_COUNT = 17 };

typedef struct owner_list {
    effect_heap_node_t* head;
    s32 _unused_04;
} owner_list_t;

/* Heap block lists and allocation bounds. */
static effect_heap_node_t* s_rover;

static owner_list_t s_owner_lists[EFFECT_HEAP_OWNER_COUNT] = { 0 };

static effect_heap_node_t* s_heap_base = 0;

static s16 s_block_count = 0;

/* Lowest largest-free-block size seen, in 8-byte units. */
static s32 s_min_largest_free_block;

/* Provisional: never written; effect_heap_print_stats prints it x4 in its "DP" line. */
extern s32 g_effect_heap_debug_dp_words;

/* Provisional: never written; added to the "DP" figure of effect_heap_print_stats. */
extern s32 g_effect_heap_debug_dp_bytes;

/* Make `buffer` the battle heap: a single free block of buffer_size / 8 - 1 eight-byte
 * blocks that forms the free ring by itself, with owner lists 0 to 16 emptied. */
void effect_heap_init(effect_heap_node_t* buffer, u32 buffer_size) {
    s32 owner;
    s16 block_count = (buffer_size >> 3) - 1;

    s_rover = buffer;
    s_heap_base = buffer;
    buffer->size = block_count;
    s_block_count = block_count;
    buffer->next = buffer;
    for (owner = EFFECT_HEAP_OWNER_COUNT - 1; owner >= 0; owner--) {
        s_owner_lists[owner].head = 0;
    }
}

/* Allocate `size` bytes from the battle heap for owner list `owner` (0-16) and
 * return the address past the block header.
 *
 * First fit along the circular free list, starting after the rover as in the K&R
 * malloc: the request is rounded up to 8-byte units plus one unit for the header,
 * an exact fit is unlinked and a larger block gives up its tail. The block goes
 * to the head of the owner's list. There is no failure exit: when no free block
 * is large enough the search goes round the list without end. */
void* effect_heap_alloc_block(u32 size, s32 owner) {
    effect_heap_node_t* prev;
    effect_heap_node_t* node;
    u32 blocks = (size + 15) >> 3;

    prev = s_rover;
    node = prev->next;
    for (;;) {
        if (node->size >= blocks) {
            if (node->size == blocks) {
                prev->next = node->next;
            } else {
                node->size = node->size - blocks;
                node += node->size;
                node->size = blocks;
            }
            node->owner = owner;
            node->next = s_owner_lists[owner].head;
            s_rover = prev;
            s_owner_lists[owner].head = node;
            return node + 1;
        }
        prev = node;
        node = node->next;
    }
}

/*
 * Release a block from effect_heap_alloc_block.
 *
 * The block is unlinked from its owner's list, then returned to the circular
 * free list in address order and coalesced with both neighbors, as in the
 * K&R free().
 */
void effect_heap_free_block(void* ptr) {
    effect_heap_node_t* node = (effect_heap_node_t*)ptr - 1;
    owner_list_t* list = &s_owner_lists[node->owner];
    effect_heap_node_t* owned = list->head;
    effect_heap_node_t* prev;
    effect_heap_node_t* free_node;

    if (owned == node) {
        list->head = node->next;
    } else {
        for (prev = owned; (owned = prev->next) != node; prev = owned) { }
        prev->next = node->next;
    }

    for (free_node = s_rover; !(node > free_node && node < free_node->next); free_node = free_node->next) {
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
    s_rover = free_node;
}

/*
 * Debug-print battle heap usage: total and largest free block on the free
 * ring, the low-water mark of the largest block, a "DP" value, and the bytes
 * held by owners 0 through 15. The original diagnostic omits owner 16.
 *
 * The ring-end test rereads g_rover; GCC hoists that load, which
 * produces the target copy of the rover into a separate register.
 */
void effect_heap_print_stats(void) {
    effect_heap_node_t* free_node;
    effect_heap_node_t* node;
    s32 total = 0;
    s32 largest = 0;
    s32 owner;
    s32 used;
    s32 size;

    free_node = s_rover->next;
    for (;;) {
        size = free_node->size;
        total += size;
        if (largest < size) {
            largest = size;
        }
        if (free_node == s_rover) {
            break;
        }
        free_node = free_node->next;
    }
    FntPrint("HEAP REMAIN %x MAX %x\n", total * 8, largest * 8);
    if (largest < s_min_largest_free_block) {
        s_min_largest_free_block = largest;
    }
    FntPrint("MIN HEAP %x ", s_min_largest_free_block * 8);
    FntPrint("DP %x\n", g_effect_heap_debug_dp_words * 4 + g_effect_heap_debug_dp_bytes);
    for (owner = 0; owner < 16; owner++) {
        used = 0;
        for (node = s_owner_lists[owner].head; node != 0; node = node->next) {
            used += node->size;
        }
        if (used != 0) {
            FntPrint("TASK %d %x\n", owner, used * 8);
        }
    }
}
