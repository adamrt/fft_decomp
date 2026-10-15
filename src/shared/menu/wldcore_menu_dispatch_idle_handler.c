#include "fft/wldcore.h"

void wldcore_menu_dispatch_idle_handler(void) {
    s32 index;

    index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_idle_handlers[g_wldcore_menu_stack_types[index]](&g_wldcore_menu_stack_records[index]);
}
