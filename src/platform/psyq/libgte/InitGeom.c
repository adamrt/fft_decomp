#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001beb8–0x8001bf38: patch exception entry, enable COP2 and seed
 * geometry control registers. Return address uses original global storage. */
void InitGeom(void) {
    s32 status;
    register s32 value __asm__(
        "$8"); /* Unpinned control constants use a saved register and create a nonretail frame. */
    PSYQ_CPU_GLOBAL_SAVE_RA(g_psyq_gte_init_saved_ra);
    PSYQ_CPU_GLOBAL_CALL(_patch_gte);
    PSYQ_CPU_GLOBAL_RESTORE_RA(g_psyq_gte_init_saved_ra);
    PSYQ_CPU_STATUS_READ(status);
    status |= PSYQ_GTE_COP2_ENABLE;
    PSYQ_CPU_STATUS_WRITE(status);
    PSYQ_CPU_SIGNED_CONSTANT(value, 341);
    PSYQ_GTE_CONTROL_WRITE_NOP(value, PSYQ_GTE_CTRL_ZSF3);
    PSYQ_CPU_SIGNED_CONSTANT(value, 256);
    PSYQ_GTE_CONTROL_WRITE_NOP(value, PSYQ_GTE_CTRL_ZSF4);
    PSYQ_CPU_SIGNED_CONSTANT(value, 1000);
    PSYQ_GTE_CONTROL_WRITE_NOP(value, PSYQ_GTE_CTRL_H);
    value = -4194;
    PSYQ_GTE_CONTROL_WRITE_NOP(value, PSYQ_GTE_CTRL_DQA);
    value = 0x01400000;
    PSYQ_GTE_CONTROL_WRITE_NOP(value, PSYQ_GTE_CTRL_DQB);
    PSYQ_GTE_ZERO_OFFSETS();
}
