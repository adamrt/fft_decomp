#ifndef PSX_LIBCARD_H
#define PSX_LIBCARD_H

#include "psx/bios.h"
#include "psx/libapi.h"

/* Original resident card wrappers and BIOS patch helpers. */
void _patch_card(void);
void _patch_card2(void);
void psyq_card_restore_exception_prefix(void);

/* The BIOS supplies the live v1 base. At the hardware I/O base the two words
 * read are the controller port status (JOY_STAT) and the interrupt mask
 * (I_MASK). */
typedef struct {
    u8 _unused_0000[0x1044];
    u32 joy_status; /* 0x1044 */
    u8 _unused_1048[0x2c];
    u32 interrupt_mask; /* 0x1074 */
} psyq_card_interrupt_view_t;

typedef struct {
    void* _unknown_000[PSYQ_BIOS_B_CHANGE_CLEAR_PAD];
    u32* change_clear_pad;
} psyq_card_bios_b0_table_t;

enum {
    PSYQ_CARD_IRQ_PENDING_MASK = 0x80,
    PSYQ_CARD_EXCEPTION_ADDRESS_HIGH_WORD = 0x70 / 4,
    PSYQ_CARD_EXCEPTION_ADDRESS_LOW_WORD = 0x74 / 4,
    PSYQ_CARD_INTERRUPT_PATCH_WORD = 0x28 / 4,
    PSYQ_CARD_PAD_DELAY_PATCH_WORD = 0x9c8 / 4,
};

extern u32 g_psyq_card_patch_saved_ra;
extern u32 g_psyq_card_restore_saved_ra;
extern void* g_psyq_card_patch_continuation;
extern u32 g_psyq_card_interrupt_jump_template[];
extern u32 g_psyq_card_interrupt_jump_template_end[];
extern u32 g_psyq_card_delay_template[];
extern u32 g_psyq_card_delay_template_end[];
extern u32 g_psyq_card_restore_template[];
extern u32 g_psyq_card_restore_template_end[];

void psyq_card_interrupt_tail(void);
void psyq_card_short_delay(void);
void psyq_card_long_delay(void);

#endif
