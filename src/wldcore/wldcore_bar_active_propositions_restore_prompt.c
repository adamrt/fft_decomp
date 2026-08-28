#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_bar_active_propositions_restore_prompt(void) {
    g_wldcore_menu_result = 0x160;
    world_thread_set_parameters(14, 0x19, 0xB81B, 0);
}
