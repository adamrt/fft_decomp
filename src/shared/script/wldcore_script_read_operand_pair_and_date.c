#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Read the next two script operands (a month and a day) and the calendar's
 * current month and day. */
void wldcore_script_read_operand_pair_and_date(
    s32* out_month, s32* out_day, s32* out_current_month, s32* out_current_day) {
    u16* ipp;
    u16 ip;

    ipp = &g_wldcore_script_state.ip;
    *out_month = g_wldcore_script_state.data[*ipp];
    ip = *ipp + 1;
    *ipp = ip;
    *out_day = g_wldcore_script_state.data[ip];
    *ipp = *ipp + 1;
    *out_current_month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    *out_current_day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
}
