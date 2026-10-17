#include "psx/gte_inline.h"
#include "psx/libgte.h"

#include "psx/cpu_return_abi.h"

/* main 0x8001bfc8–0x8001c054: inverse-root mantissa/exponent table lookup. */
void InvSquareRoot(long value, long* mantissa_out, long* exponent_out) {
    register s32 odd __asm__("$8");
    register s32 exponent __asm__("$9");
    register s32 even_leading __asm__("$10");
    register s32 difference __asm__("$11");
    register s32 normalized __asm__("$12");
    register s32 factor __asm__("$13");
    register s32 limit __asm__("$1");
    register u32 table_base __asm__("$13");
    register s32 zero __asm__("$0");
    PSYQ_GTE_LZCS(value, psyq_cpu_return_value);
    PSYQ_CPU_SIGNED_CONSTANT(limit, 32);
    if (psyq_cpu_return_value == limit)
        goto zero_result;
    if (psyq_cpu_return_value == 0)
        goto zero_result;
    __asm__ volatile("" : : : "memory");
    odd = psyq_cpu_return_value & 1;
    even_leading = -2;
    even_leading = psyq_cpu_return_value & even_leading;
    __asm__(""
        : "=r"(odd), "=r"(even_leading)
        : "0"(odd), "1"(even_leading)); /* Retail retains the parity scratch operation. */
    PSYQ_CPU_SIGNED_CONSTANT(exponent, 31);
    PSYQ_CPU_TRAP_SUB(exponent, exponent, even_leading);
    exponent >>= 1;
    PSYQ_CPU_TRAP_ADDI(difference, even_leading, -24);
    if (difference >= 0) {
        __asm__ volatile("" : : : "memory"); /* Preserve the empty BLTZ delay slot. */
        normalized = value << difference;
        PSYQ_CPU_SHARED_DELAY_BEGIN();
        if (zero == 0)
            goto normalized_ready;
    }
    difference = zero + 24;
    PSYQ_CPU_SHARED_DELAY_END();
    PSYQ_CPU_TRAP_SUB(difference, difference, even_leading);
    normalized = value >> difference;
normalized_ready:
    PSYQ_CPU_TRAP_ADDI(normalized, normalized, -64);
    normalized <<= 1;
    PSYQ_GTE_TABLE_HIGH_PAIRED(table_base, g_psyq_gte_inv_sqrt_table);
    table_base += normalized;
    __asm__("" : "=r"(table_base) : "0"(table_base)); /* Keep the low-half table load as the relocation target. */
    factor = *(s16*)table_base;                       /* Retail splits this symbol relocation across LUI and LH. */
    *exponent_out = exponent;
    *mantissa_out = factor;
    __asm__ volatile("" : : : "memory"); /* Finish both outputs before the return-code load; retain JR NOP. */
    PSYQ_CPU_SIGNED_CONSTANT(psyq_cpu_return_value, 1);
    goto* psyq_cpu_return_address;
zero_result:
    psyq_cpu_return_value = -1;
    goto* psyq_cpu_return_address;
}
