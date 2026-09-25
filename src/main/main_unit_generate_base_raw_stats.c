#include "fft/main.h"

/*
 * Rolls the five 24-bit raw stats (3 bytes each, little-endian) for the
 * selected unit type: generic male, generic female, Ramza, or monster.
 */
void main_unit_generate_base_raw_stats(u8* raw_stats, int unit_type) {
    /* Pin required: unpinned, type_index, index and base rotate among $s2/$s3/$s4. */
    register int type_index __asm__("$20") = unit_type;
    int index = 0;
    u8* variance = (u8*)g_main_unit_generation_raw_stat_variance;
    volatile u8* out = raw_stats;
    u8* table = (u8*)g_main_unit_generation_base_data;
    u8* base = (u8*)(type_index * 12 + (u32)table);

    do {
        int value = base[0] << 14;

        /* Keep the type-index multiplication inside the loop, as in the target. */
        __asm__("" : "=r"(type_index) : "0"(type_index));
        value += (rand() * (variance + type_index * UNIT_RAW_STAT_COUNT)[index]) / 2;
        base++;
        index++;
        out[0] = value;
        out[1] = (u32)value >> 8;
        out[2] = value >> 16;
        out += UNIT_RAW_STAT_SERIALIZED_BYTES;
    } while (index < UNIT_RAW_STAT_COUNT);
}
