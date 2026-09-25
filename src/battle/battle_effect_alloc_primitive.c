#include "fft/battle.h"

enum {
    BATTLE_EFFECT_PRIMITIVE_BUFFER_SIZE = 0x8000,
};

/* Allocate one typed primitive from the effect renderer's circular arena.
 *
 * The original renderer wraps before an allocation that would cross the
 * 0x8000-byte arena boundary; callers consume the result immediately. */
void* battle_effect_alloc_primitive(s32 primitive_type) {
    /* These registers retain the arena offset and primitive size across the wrap test. */
    register s32 offset __asm__("$3");
    register s32 size __asm__("$4");
    register s32 size_byte_offset __asm__("$2");
    register s32 remaining __asm__("$2");

    offset = g_battle_effect_prim_buffer_offset;
    /* Preserve the arena offset load before indexing the size table. */
    __asm__("" : "=r"(offset) : "0"(offset));
    size_byte_offset = primitive_type << 2;
    /* Byte indexing keeps the table address in $v0 before the size load. */
    size = *(s32*)((u8*)g_battle_effect_primitive_sizes + size_byte_offset);
    /* Keep the size load ahead of the buffer-limit constant. */
    __asm__("" : "=r"(size) : "0"(size));
    remaining = BATTLE_EFFECT_PRIMITIVE_BUFFER_SIZE - offset;
    if (remaining < size) {
        g_battle_effect_prim_buffer_offset = size;
        offset = 0;
    } else {
        remaining = size + offset;
        g_battle_effect_prim_buffer_offset = remaining;
    }
    return g_battle_effect_prim_buffer + offset;
}
