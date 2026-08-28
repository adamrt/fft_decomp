#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_list_script_variables_restore_prompt(void) {
    world_thread_set_parameters(14, 0x19, 0xB848, 0);
    g_wldcore_menu_result = 0x160;
}
