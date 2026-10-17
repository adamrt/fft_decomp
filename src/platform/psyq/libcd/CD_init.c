/* SCUS_942.21 0x800202f8..0x800204e3. */
#include "psx/libcd.h"
#include "psx/libetc.h"

/* Initialize libcd and the drive: install the CD interrupt handler, flush pending controller
 * interrupts (as CD_flush), then send Nop (twice when the lid reads open), Reset and Demute.
 * Returns 0, or -1 when a step fails. */
s32 CD_init(void) {
    puts(g_psyq_cd_init_message);
    printf(g_psyq_cd_init_address_format, &g_psyq_cd_init_irq_state_pointer);
    CD_com = 0;
    CD_mode = 0;
    CD_cbready = 0;
    CD_cbsync = 0;
    CD_status1 = 0;
    CD_status = 0;
    ResetCallback();
    InterruptCallback(2, callback);
    *g_psyq_cd_index_status_register = 1;
    while (*g_psyq_cd_request_interrupt_register & 7) {
        *g_psyq_cd_index_status_register = 1;
        *g_psyq_cd_request_interrupt_register = 7;
        *g_psyq_cd_parameter_data_register = 7;
    }
    g_psyq_cd_irq_state.ready = g_psyq_cd_irq_state.second_ready = 0;
    g_psyq_cd_irq_state.sync = CdlComplete;
    *g_psyq_cd_index_status_register = 0;
    *g_psyq_cd_request_interrupt_register = 0;
    *g_psyq_cd_common_delay_register = 0x1325;
    CD_cw(CdlNop, 0, 0, 0);
    if (CD_status & CdlStatShellOpen)
        CD_cw(CdlNop, 0, 0, 0);
    if (CD_cw(CdlReset, 0, 0, 0))
        return -1;
    if (CD_cw(CdlDemute, 0, 0, 0))
        return -1;
    return CD_sync(0, 0) == CdlComplete ? 0 : -1;
}
