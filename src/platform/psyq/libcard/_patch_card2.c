#include "psx/card_abi_inline.h"
#include "psx/libcard.h"

/* main 0x800289d4–0x80028a40: swap five PAD-clear handler 32-bit instructions. */
void _patch_card2(void) {
    /* Pins preserve the original scratch registers and call ABI. */
    psyq_card_bios_b0_table_t* table;
    u32* destination;
    register u32 old_word __asm__("$3");
    register u32 replacement __asm__("$8");
    register u32* source __asm__("$10");
    register u32* end __asm__("$9");
    PSYQ_CPU_GLOBAL_SAVE_RA(g_psyq_card_patch_saved_ra);
    PSYQ_CPU_GLOBAL_CALL(EnterCriticalSection);
    PSYQ_CARD_BIOS_TABLE(table, PSYQ_BIOS_B_GET_B0_TABLE);
    destination = table->change_clear_pad;
    PSYQ_CPU_ADDRESS_HIGH(source, g_psyq_card_delay_template);
    PSYQ_CPU_ADDRESS_HIGH(end, g_psyq_card_delay_template_end);
    old_word = destination[PSYQ_CARD_PAD_DELAY_PATCH_WORD];
    __asm__ volatile("" : : "r"(old_word)); /* Retain the original unused prefetch in v1. */
    PSYQ_CPU_ADDRESS_LOW(source, g_psyq_card_delay_template);
    PSYQ_CPU_ADDRESS_LOW(end, g_psyq_card_delay_template_end);
    do {
        old_word = destination[PSYQ_CARD_PAD_DELAY_PATCH_WORD];
        replacement = *source++;
        source[-1] = old_word;
        __asm__ volatile(""
            :
            :
            : "memory"); /* The swapped 32-bit template value is saved before advancing the BIOS pointer. */
        destination++;
        __asm__("" : "=r"(destination) : "0"(destination)); /* Original store uses the advanced code pointer. */
        destination[PSYQ_CARD_PAD_DELAY_PATCH_WORD - 1] = replacement;
    } while (source != end);
    PSYQ_CPU_GLOBAL_CALL(FlushCache);
    PSYQ_CPU_GLOBAL_RESTORE_RA(g_psyq_card_patch_saved_ra);
}
