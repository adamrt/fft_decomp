/* SCUS_942.21 0x800200d4..0x800201b3. */
#include "psx/libcd.h"

/* Acknowledge every pending CD controller interrupt and reset the sync state to complete and the
 * ready states to none. */
void CD_flush(void) {
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
}
