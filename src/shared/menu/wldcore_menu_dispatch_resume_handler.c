#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_menu_dispatch_resume_handler(void) {
    s32 index;

    index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_resume_handlers[g_wldcore_menu_stack_types[index]](&g_wldcore_menu_stack_records[index]);
}
