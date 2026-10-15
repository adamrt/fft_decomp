#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/open.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

void wldcore_menu_dispatch_idle_handler(void) {
    s32 index;

    index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_idle_handlers[g_wldcore_menu_stack_types[index]](&g_wldcore_menu_stack_records[index]);
}
