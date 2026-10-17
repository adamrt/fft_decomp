/* SCUS_942.21 0x8001fc04..0x8002004b. */
#include "psx/libcd.h"
#include "psx/libetc.h"

/* Issue a CD command after the previous one completes. Unless asynchronous, wait for the
 * controller's first response and copy it to result; also caches the CdlSetloc position and, on
 * success, the CdlSetmode mode. Returns 0, -1 on a disk error or timeout, -2 for missing
 * parameters. */
s32 CD_cw(u8 command, const u8* parameter, u8* result, s32 asynchronous) {
    s32 index;
    if (CD_debug > 1)
        printf(g_psyq_cd_command_trace_format, CD_comstr[command]);
    if (g_psyq_cd_command_attributes.parameter_counts[command] && !parameter) {
        if (CD_debug > 0)
            printf(g_psyq_cd_missing_parameters_format, CD_comstr[command]);
        return -2;
    }
    CD_sync(0, 0);
    if (command == CdlSetloc) {
        /* Cache the serialized CdlLOC bytes; separate member stores change the original loop. */
        for (index = 0; index < 4; index++)
            ((u8*)&CD_pos)[index] = parameter[index];
    }
    g_psyq_cd_irq_state.sync = 0;
    if (g_psyq_cd_command_attributes.resets_ready[command])
        g_psyq_cd_irq_state.ready = 0;
    *g_psyq_cd_index_status_register = 0;
    for (index = 0; index < g_psyq_cd_command_attributes.parameter_counts[command]; index++)
        *g_psyq_cd_parameter_data_register = parameter[index];
    CD_com = command;
    *g_psyq_cd_command_response_register = command;
    if (asynchronous)
        return 0;
    psyq_cd_set_timeout(g_psyq_cd_command_timeout_name);
    while (g_psyq_cd_irq_state.sync == 0) {
        if (psyq_cd_check_timeout())
            return -1;
        if (CheckCallback())
            psyq_cd_dispatch_callbacks();
    }
    if (g_psyq_cd_irq_state.sync == CdlComplete && command == CdlSetmode)
        CD_mode = *parameter;
    psyq_cd_copy_result(result, g_psyq_cd_sync_result, sizeof(g_psyq_cd_sync_result));
    return -(g_psyq_cd_irq_state.sync == CdlDiskError);
}
