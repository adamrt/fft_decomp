/* SCUS_942.21 0x80014358..0x800143ab. */
#include "fft/suzuki.h"

/* Unlink the heap block that owns payload from the driver's block list; its space becomes a gap
 * for suzuki_sound_alloc. The block must be in the list. */
void suzuki_sound_free(void* payload) {
    suzuki_heap_block_t* block = (suzuki_heap_block_t*)payload - 1;
    suzuki_heap_block_t* current = g_suzuki_sound_heap_blocks;
    suzuki_heap_block_t* previous = 0;
    while (current != block) {
        previous = current;
        current = previous->next;
    }
    if (previous != 0) {
        previous->next = block->next;
    } else {
        g_suzuki_sound_heap_blocks = g_suzuki_sound_heap_blocks->next;
    }
}
