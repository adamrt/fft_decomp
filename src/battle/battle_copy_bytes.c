#include "fft/main_heap.h"
#include "psx/types.h"

/* Copy a nonempty byte sequence from source to destination.
 *
 * The target performs the first byte copy before testing the decremented
 * count, so callers must provide a positive byte count. */
void battle_copy_bytes(void* destination, const void* source, s32 count) {
    /* Pin: unpinned, GCC copies the byte through $v0 instead of $a3. */
    register u8 value __asm__("$7");
    u8* destination_bytes = destination;
    const u8* source_bytes = source;

    /* The retail loop uses trapping addi updates, a nop after the lbu and an
     * unfilled bnez delay slot. GCC otherwise emits addiu and fills both. */
    do {
        value = *source_bytes;
        *destination_bytes = value;
        __asm__ volatile("addi %0,%0,1" : "=r"(destination_bytes) : "0"(destination_bytes));
        __asm__ volatile("addi %0,%0,1" : "=r"(source_bytes) : "0"(source_bytes));
        __asm__ volatile("addi %0,%0,-1" : "=r"(count) : "0"(count));
    } while (count);
}
