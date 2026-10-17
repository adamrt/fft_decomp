#include "psx/cpu_return_address_abi.h"
#include "psx/gte_inline.h"
#include "psx/libc.h"
#include "psx/libgte.h"

/* main 0x8001cba4–0x8001cc44: restore the eight rotation/translation controls. */
void PopMatrix(void) {
    /* Fixed scratch registers reproduce the handwritten COP2/ABI allocation. */
    register s32 offset __asm__("$14");
    register MATRIX* frame __asm__("$15");
    register s32 first __asm__("$8");
    register s32 second __asm__("$9");
    register s32 third __asm__("$10");
    register u32 save_page __asm__("$1");
    offset = g_psyq_gte_matrix_stack_offset;
    if (offset > 0)
        goto restore_frame;
    save_page = PSYQ_GTE_MATRIX_STACK_RA_PAGE;
    PSYQ_GTE_GLOBAL_SAVE_RA_PAGE(save_page, g_psyq_gte_matrix_stack_saved_ra);
    PSYQ_GTE_GLOBAL_PRINTF(printf, g_psyq_gte_matrix_stack_underflow_message);
    PSYQ_CPU_GLOBAL_RESTORE_RA(g_psyq_gte_matrix_stack_saved_ra);
    goto* psyq_cpu_return_address;
restore_frame:
    PSYQ_CPU_TRAP_ADDI(offset, offset, -32);
    g_psyq_gte_matrix_stack_offset = offset;
    PSYQ_CPU_ADDRESS_HIGH(frame, g_psyq_gte_matrix_stack);
    frame = (MATRIX*)((u32)frame + offset);
    PSYQ_CPU_ADDRESS_LOW(frame, g_psyq_gte_matrix_stack);
    /* Rotation controls transfer 32-bit values; R33 includes MATRIX's 16-bit alignment field. */
    first = *(u32*)&frame->m[0][0];
    second = *(u32*)&frame->m[0][2];
    PSYQ_GTE_CTC2(PSYQ_GTE_CTRL_R11_R12, first);
    PSYQ_GTE_CTC2(PSYQ_GTE_CTRL_R13_R21, second);
    first = *(u32*)&frame->m[1][1];
    second = *(u32*)&frame->m[2][0];
    PSYQ_GTE_CTC2(PSYQ_GTE_CTRL_R22_R23, first);
    PSYQ_GTE_CTC2(PSYQ_GTE_CTRL_R31_R32, second);
    first = *(u32*)&frame->m[2][2];
    PSYQ_GTE_CONTROL_WRITE_NOP(first, PSYQ_GTE_CTRL_R33);
    first = frame->t[0];
    second = frame->t[1];
    third = frame->t[2];
    PSYQ_GTE_CTC2(PSYQ_GTE_CTRL_TRX, first);
    PSYQ_GTE_CTC2(PSYQ_GTE_CTRL_TRY, second);
    PSYQ_GTE_CTC2(PSYQ_GTE_CTRL_TRZ, third);
    goto* psyq_cpu_return_address;
}
