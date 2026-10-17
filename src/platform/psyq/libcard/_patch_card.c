#include "psx/card_abi_inline.h"
#include "psx/libcard.h"

/* main 0x8002891c–0x800289a4: install the interrupt-tail jump and continuation. */
void _patch_card(void) {
    /* Pins preserve the original scratch registers and call ABI. */
    psyq_bios_c0_table_t* table;
    register u32* destination __asm__("$2");
    u32 word;
    register u32 first __asm__("$9");
    register u32 second __asm__("$10");
    register u32* source __asm__("$10");
    register u32* end __asm__("$9");
    PSYQ_CPU_GLOBAL_SAVE_RA(g_psyq_card_patch_saved_ra);
    PSYQ_CPU_GLOBAL_CALL(EnterCriticalSection);
    PSYQ_CARD_BIOS_TABLE(table, PSYQ_BIOS_B_GET_C0_TABLE);
    destination = table->exception_handler;
    PSYQ_CPU_LOAD_WAIT();
    word = ((volatile u32*)destination)[PSYQ_CARD_EXCEPTION_ADDRESS_HIGH_WORD];
    PSYQ_CPU_LOAD_WAIT();
    first = word & 0xffff;
    word = ((volatile u32*)destination)[PSYQ_CARD_EXCEPTION_ADDRESS_LOW_WORD];
    first <<= 16;
    second = word & 0xffff;
    word = first + second;
    PSYQ_CPU_ADDRESS_HIGH(source, g_psyq_card_interrupt_jump_template);
    PSYQ_CPU_ADDRESS_HIGH(end, g_psyq_card_interrupt_jump_template_end);
    destination = (u32*)word + PSYQ_CARD_INTERRUPT_PATCH_WORD;
    PSYQ_CPU_ADDRESS_LOW(source, g_psyq_card_interrupt_jump_template);
    PSYQ_CPU_ADDRESS_LOW(end, g_psyq_card_interrupt_jump_template_end);
    do {
        word = *source++;
        destination++;
        destination[-1] = word;
    } while (source != end);
    PSYQ_CARD_FLUSH_AND_SAVE_CONTINUATION(FlushCache, g_psyq_card_patch_continuation, destination);
    PSYQ_CPU_GLOBAL_RESTORE_RA(g_psyq_card_patch_saved_ra);
}
