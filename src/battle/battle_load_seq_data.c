#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Resolve a SEQ record's two header halfwords and its 256 packed little-endian
 * offsets into blob pointers, then append the record's payload to the blob and
 * advance the allocation cursor. An entry of -1 resolves to the blob start.
 *
 * This is a near twin of battle_gfx_resolve_packed_offset_table at 0x800872ec,
 * which resolves the same record shape without the two leading header words.
 *
 * Matching constraints, all zero-instruction:
 *  - `original_source` is a second live copy of the record pointer (`move
 *    t2,a1`), which the twin's header also records as needing its own variable.
 *  - `unused` reserves the 8-byte frame that no instruction touches; only a
 *    declared local aggregate reproduces it.
 *  - `sentinel` and `out` exist so the -1 materialisation and the destination
 *    copy are ordinary statements rather than a loop.c preheader hoist and a
 *    prologue argument copy; that is what places them 3rd and 4th.
 *  - The three empty `__asm__ volatile("")` barriers split the setup into the
 *    scheduling regions that keep those six instructions in source order. With
 *    a single trailing barrier the whole block is one region and sched1
 *    re-sorts it.
 *  - The tied launder on `offset_source` stops loop.c strength-reducing the
 *    four byte reads into a second biased induction variable.
 *  - `count` is pinned to $6: the allocator otherwise gives it $7 and
 *    `offset_source` $6; splitting it per loop or reordering declarations
 *    does not move it.

 *  - The pin makes `count` dead at the final add, so the compiler folds the
 *    result into $6; the trailing volatile keepalive holds it live so the sum
 *    lands in $2 as the target has it. A non-volatile keepalive does not.
 *  - `animation_data` is an integer, not u8*: that both puts the running offset
 *    in $2 and gives `addu v0,v0,t1` its operand order. */
void battle_load_seq_data(u32* destination, const u8* source) {
    const u8* original_source;
    const u8* offset_source;
    register u32 count asm("$6");
    u32 animation_data;
    u32 animation_offset;
    u32 length;
    u32 sentinel;
    u32* out;
    volatile s32 unused[2];

    original_source = source;
    __asm__ volatile("");
    count = 0;
    sentinel = 0xffffffff;
    __asm__ volatile("");
    out = destination;
    offset_source = source + 4;
    animation_data = (u32)g_battle_gfx_load_data_cursor;
    __asm__ volatile("");
    out[0] = source[0] + (source[1] << 8);
    out[1] = source[2] + (source[3] << 8);
    do {
        animation_offset
            = offset_source[0] + (offset_source[1] << 8) + (offset_source[2] << 16) + (offset_source[3] << 24);
        if (animation_offset == sentinel) {
            animation_offset = 0;
        }
        out[2] = animation_offset + animation_data;
        offset_source += 4;
        __asm__("" : "=r"(offset_source) : "0"(offset_source));
        count++;
        out++;
    } while (count < 0x100);

    length = original_source[0x404] + (original_source[0x405] << 8);
    count = 0;
    if (length != 0) {
        source += 0x406;
        do {
            g_battle_gfx_load_data_cursor[count] = *source;
            count++;
            source++;
        } while (count < length);
    }
    g_battle_gfx_load_data_cursor += count;
    __asm__ volatile("" ::"r"(count));
}
