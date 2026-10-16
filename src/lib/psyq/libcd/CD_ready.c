/* SCUS_942.21 0x8001f938..0x8001fc03. */
#include "psx/libcd.h"
#include "psx/libetc.h"

/* Wait (mode 0) or poll once for a data-ready event, reporting a pending data end before a data
 * ready; copies the response to result and returns the event, 0 when polling finds none, -1 on
 * timeout. */
s32 CD_ready(s32 mode, u8* result) {
    s32 event;
    psyq_cd_set_timeout(g_psyq_cd_ready_timeout_name);
    while (1) {
        if (psyq_cd_check_timeout())
            return -1;
        if (CheckCallback())
            psyq_cd_dispatch_callbacks();
        event = g_psyq_cd_irq_state.second_ready;
        if (event) {
            g_psyq_cd_irq_state.second_ready = 0;
            psyq_cd_copy_result(result, g_psyq_cd_second_ready_result, 8);
            return event;
        }
        event = g_psyq_cd_irq_state.ready;
        if (event) {
            g_psyq_cd_irq_state.ready = 0;
            psyq_cd_copy_result(result, g_psyq_cd_ready_result, 8);
            return event;
        }
        if (mode)
            return 0;
    }
}
