/* SCUS_942.21 0x8001f140..0x8001f6b7. */
#include "psx/libcd.h"

static __inline__ void cd_copy_result(u8* destination, const volatile u8* source, u32 size) {
    u8* output = destination;
    const volatile u8* input = source;
    if (!output)
        return;
    while (size--)
        *output++ = *input++;
}

/* Read and acknowledge one CD controller interrupt: copy up to 8 response bytes, update CD_status
 * (counting lid openings in CD_nopen) and record the sync or ready result. Returns 0 for none or
 * unknown, 1 while the command still awaits completion, 2 when it finished, 4 for data ready or
 * data end, 6 for a disk error. */
s32 getintr(void) {
    /* Volatile keeps the stack interrupt byte and separate status-mask calculation. */
    volatile u8 interrupt;
    volatile u8 response[8];
    s32 length;
    s32 error_flags = 0;
    *g_psyq_cd_index_status_register = 1;
    interrupt = *g_psyq_cd_request_interrupt_register & 7;
    if (!interrupt)
        return 0;
    while (interrupt != (*g_psyq_cd_request_interrupt_register & 7))
        interrupt = *g_psyq_cd_request_interrupt_register & 7;
    for (length = 0; length < 8; length++) {
        if (!(*g_psyq_cd_index_status_register & 0x20))
            break;
        response[length] = *g_psyq_cd_command_response_register;
    }
    {
        s32 padding;
        for (padding = length; padding < 8; padding++)
            response[padding] = 0;
    }
    *g_psyq_cd_index_status_register = 1;
    *g_psyq_cd_request_interrupt_register = 7;
    *g_psyq_cd_parameter_data_register = 7;
    if (interrupt != CdlAcknowledge || g_psyq_cd_command_attributes.ack_updates_status[CD_com]) {
        if (!(CD_status & CdlStatShellOpen) && (response[0] & CdlStatShellOpen))
            CD_nopen++;
        CD_status = response[0];
        CD_status1 = response[1];
        error_flags = CD_status;
        error_flags &= 0x1d;
    }
    if (interrupt == CdlDiskError) {
        puts(g_psyq_cd_disk_error_message);
        if (CD_debug > 0)
            printf(g_psyq_cd_disk_error_format, CD_comstr[CD_com], CD_status, CD_status1);
    }
    switch (interrupt) {
    case CdlAcknowledge:
        if (error_flags) {
            g_psyq_cd_irq_state.sync = CdlDiskError;
            cd_copy_result(g_psyq_cd_sync_result, response, 8);
            return 2;
        }
        if (g_psyq_cd_command_attributes.waits_for_complete[CD_com]) {
            g_psyq_cd_irq_state.sync = CdlAcknowledge;
            cd_copy_result(g_psyq_cd_sync_result, response, 8);
            return 1;
        }
        g_psyq_cd_irq_state.sync = CdlComplete;
        cd_copy_result(g_psyq_cd_sync_result, response, 8);
        return 2;
    case CdlComplete:
        g_psyq_cd_irq_state.sync = error_flags ? CdlDiskError : CdlComplete;
        cd_copy_result(g_psyq_cd_sync_result, response, 8);
        return 2;
    case CdlDataReady:
        if (error_flags && length == 1)
            error_flags = 0;
        g_psyq_cd_irq_state.ready = error_flags ? CdlDiskError : CdlDataReady;
        cd_copy_result(g_psyq_cd_ready_result, response, 8);
        *g_psyq_cd_index_status_register = 0;
        *g_psyq_cd_request_interrupt_register = 0;
        return 4;
    case CdlDataEnd:
        g_psyq_cd_irq_state.ready = g_psyq_cd_irq_state.second_ready = CdlDataEnd;
        cd_copy_result(g_psyq_cd_second_ready_result, response, 8);
        cd_copy_result(g_psyq_cd_ready_result, response, 8);
        return 4;
    case CdlDiskError:
        g_psyq_cd_irq_state.sync = g_psyq_cd_irq_state.ready = CdlDiskError;
        cd_copy_result(g_psyq_cd_sync_result, response, 8);
        cd_copy_result(g_psyq_cd_ready_result, response, 8);
        return 6;
    default:
        puts(g_psyq_cd_unknown_interrupt_message);
        printf(g_psyq_cd_unknown_interrupt_format, interrupt);
        return 0;
    }
}
