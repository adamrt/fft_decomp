#include "psx/card_abi_inline.h"
#include "psx/libcard.h"

/* main 0x80028a64–0x80028acc: restore three exception-handler 32-bit instructions. */
void psyq_card_restore_exception_prefix(void) {
    /* Pins preserve the original scratch registers and call ABI. */
    psyq_bios_c0_table_t* table;
    u32* destination;
    register u32 word __asm__("$3");
    register u32* source __asm__("$10");
    register u32* end __asm__("$9");
    PSYQ_CPU_GLOBAL_SAVE_RA(g_psyq_card_restore_saved_ra);
    PSYQ_CPU_GLOBAL_CALL(EnterCriticalSection);
    PSYQ_CARD_BIOS_TABLE(table, PSYQ_BIOS_B_GET_C0_TABLE);
    PSYQ_CPU_ADDRESS_HIGH(source, g_psyq_card_restore_template);
    PSYQ_CPU_ADDRESS_HIGH(end, g_psyq_card_restore_template_end);
    destination = table->exception_handler;
    PSYQ_CPU_ADDRESS_LOW(source, g_psyq_card_restore_template);
    PSYQ_CPU_ADDRESS_LOW(end, g_psyq_card_restore_template_end);
    do {
        word = *source++;
        destination++;
        destination[PSYQ_CARD_EXCEPTION_ADDRESS_HIGH_WORD - 1] = word;
    } while (source != end);
    PSYQ_CPU_GLOBAL_CALL(FlushCache);
    PSYQ_CPU_GLOBAL_CALL(ExitCriticalSection);
    PSYQ_CPU_GLOBAL_RESTORE_RA(g_psyq_card_restore_saved_ra);
}
