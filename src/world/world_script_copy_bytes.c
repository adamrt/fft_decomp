#include "fft/world.h"
#include "psx/types.h"

/* Copy a byte range used by WORLD.BIN data setup. */
void world_script_copy_bytes(void* destination, const void* source, s32 count) {
    /* The hand-written original moves the byte through $a3; GCC would pick $v0. */
    register u8 value __asm__("$7");
    u8* dst = destination;
    const u8* src = source;

    /* Hand-assembled in the retail binary: trapping addi updates, a nop after
     * the lbu and an unfilled bnez delay slot. GCC emits addiu and fills both. */
    do {
        value = *src;
        *dst = value;
        __asm__ volatile("addi %0,%0,1" : "=r"(dst) : "0"(dst));
        __asm__ volatile("addi %0,%0,1" : "=r"(src) : "0"(src));
        __asm__ volatile("addi %0,%0,-1" : "=r"(count) : "0"(count));
    } while (count);
}
