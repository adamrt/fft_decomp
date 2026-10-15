#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Resume handler of the active propositions list: block Circle, Cross and
 * Select for the levels underneath and show prompt 0xB81B again. */
void wldcore_bar_active_propositions_restore_prompt(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_JOBS_IN_PROGRESS, 0);
}
