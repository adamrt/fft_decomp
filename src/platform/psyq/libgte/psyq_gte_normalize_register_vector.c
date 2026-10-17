#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001c0c4–0x8001c180: normalization worker with scratch-register ABI.
 * Inputs/results are t0–t2; v0 retains the sum of squares. No zero guard exists. */
void psyq_gte_normalize_register_vector(void) {
    register s32 x __asm__("$8");
    register s32 y __asm__("$9");
    register s32 z __asm__("$10");
    register s32 square_x __asm__("$11");
    register s32 square_y __asm__("$12");
    register s32 square_z __asm__("$13");
    register s32 sum __asm__("$2");
    s32 leading;
    register s32 mask __asm__("$1");
    register s32 exponent __asm__("$14");
    register s32 difference __asm__("$11");
    register s32 normalized __asm__("$12");
    register s32 factor __asm__("$13");
    register u32 table_base __asm__("$13");
    register s32 zero __asm__("$0");
    PSYQ_GTE_NORMALIZE_SQUARE(x, y, z, square_x, square_y, square_z);
    PSYQ_CPU_TRAP_ADD(square_x, square_x, square_y);
    PSYQ_CPU_TRAP_ADD(sum, square_x, square_z);
    PSYQ_GTE_LZCS(sum, leading);
    mask = -2;
    leading &= mask;
    PSYQ_CPU_SIGNED_CONSTANT(exponent, 31);
    PSYQ_CPU_TRAP_SUB(exponent, exponent, leading);
    PSYQ_CPU_TRAP_ADDI(difference, leading, -24);
    exponent >>= 1;
    if (difference >= 0) {
        normalized = sum << difference;
        if (zero == 0)
            goto normalized_ready;
    }
    difference = zero + 24;
    PSYQ_CPU_TRAP_SUB(difference, difference, leading);
    normalized = sum >> difference;
normalized_ready:
    PSYQ_CPU_TRAP_ADDI(normalized, normalized, -64);
    normalized <<= 1;
    PSYQ_GTE_TABLE_HIGH_PAIRED(table_base, g_psyq_gte_inv_sqrt_table);
    table_base += normalized;
    __asm__("" : "=r"(table_base) : "0"(table_base));
    factor = *(s16*)table_base; /* The paired LO16 relocation supplies the table offset at this load. */
    PSYQ_GTE_NORMALIZE_SCALE(x, y, z, factor);
    x >>= exponent;
    y >>= exponent;
    z >>= exponent;
    __asm__ volatile("" : : "r"(x), "r"(y), "r"(z)); /* Keep all result shifts before the empty return delay slot. */
}
