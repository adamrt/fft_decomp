#include "fft/main.h"

/* Roll the five little-endian 24-bit raw stats for the selected unit type. */
void main_unit_generate_base_raw_stats(
    unit_raw_stat_t raw_stats[UNIT_RAW_STAT_COUNT], main_unit_generation_type_e unit_type) {
    /* Without the pin, type_index, index and base rotate among $s2/$s3/$s4. */
    register s32 type_index __asm__("$20") = unit_type;
    s32 index = 0;
    u8(*variance)[UNIT_RAW_STAT_COUNT] = g_main_unit_generation_raw_stat_variance;
    main_unit_generation_base_data_t* table = g_main_unit_generation_base_data;
    u8* base = table[type_index].raw_stat_seeds;

    do {
        s32 value = base[index] << 14;

        /* Removing the tied use changes the prologue schedule. */
        __asm__("" : "=r"(type_index) : "0"(type_index));
        value += (rand() * variance[type_index][index]) / 2;
        raw_stats[index].low = value;
        raw_stats[index].middle = (u32)value >> 8;
        raw_stats[index].high = value >> 16;
        index++;
    } while (index < UNIT_RAW_STAT_COUNT);
}
