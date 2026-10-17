#include "psx/cpu_return_address_abi.h"
#include "psx/gte_inline.h"
#include "psx/libc.h"
#include "psx/libgte.h"

/* main 0x8001cb04–0x8001cba4: save the eight rotation/translation controls. */
void PushMatrix(void) {
    /* Fixed scratch registers reproduce the handwritten COP2/ABI allocation. */
    register s32 offset __asm__("$14");
    register MATRIX* frame __asm__("$15");
    register s32 first __asm__("$8");
    register s32 second __asm__("$9");
    register s32 third __asm__("$10");
    register u32 save_page __asm__("$1");
    register s32 available __asm__("$1");
    offset = g_psyq_gte_matrix_stack_offset;
    available = offset < 640;
    if (available)
        goto save_frame;
    save_page = PSYQ_GTE_MATRIX_STACK_RA_PAGE;
    PSYQ_GTE_GLOBAL_SAVE_RA_PAGE(save_page, g_psyq_gte_matrix_stack_saved_ra);
    PSYQ_GTE_GLOBAL_PRINTF(printf, g_psyq_gte_matrix_stack_overflow_message);
    PSYQ_CPU_GLOBAL_RESTORE_RA(g_psyq_gte_matrix_stack_saved_ra);
    goto* psyq_cpu_return_address;
save_frame:
    PSYQ_CPU_ADDRESS_HIGH(frame, g_psyq_gte_matrix_stack);
    frame = (MATRIX*)((u32)frame + offset);
    PSYQ_CPU_ADDRESS_LOW(frame, g_psyq_gte_matrix_stack);
    PSYQ_GTE_CFC2(PSYQ_GTE_CTRL_R11_R12, first);
    PSYQ_GTE_CFC2(PSYQ_GTE_CTRL_R13_R21, second);
    /* Rotation controls transfer 32-bit values; R33 includes MATRIX's 16-bit alignment field. */
    *(u32*)&frame->m[0][0] = first;
    *(u32*)&frame->m[0][2] = second;
    PSYQ_GTE_CFC2(PSYQ_GTE_CTRL_R22_R23, first);
    PSYQ_GTE_CFC2(PSYQ_GTE_CTRL_R31_R32, second);
    *(u32*)&frame->m[1][1] = first;
    *(u32*)&frame->m[2][0] = second;
    PSYQ_GTE_CFC2_NOP(PSYQ_GTE_CTRL_R33, first);
    *(u32*)&frame->m[2][2] = first;
    PSYQ_GTE_CFC2(PSYQ_GTE_CTRL_TRX, first);
    PSYQ_GTE_CFC2(PSYQ_GTE_CTRL_TRY, second);
    PSYQ_GTE_CFC2(PSYQ_GTE_CTRL_TRZ, third);
    frame->t[0] = first;
    frame->t[1] = second;
    frame->t[2] = third;
    PSYQ_CPU_TRAP_ADDI(offset, offset, 32);
    g_psyq_gte_matrix_stack_offset = offset;
    goto* psyq_cpu_return_address;
}
